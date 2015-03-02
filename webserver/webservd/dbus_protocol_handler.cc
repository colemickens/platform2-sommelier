// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webserver/webservd/dbus_protocol_handler.h"

#include <base/bind.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>

#include "webserver/webservd/dbus_request_handler.h"
#include "webserver/webservd/protocol_handler.h"
#include "webserver/webservd/request.h"
#include "webserver/webservd/server.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::ExportedObjectManager;

namespace webservd {

DBusProtocolHandler::DBusProtocolHandler(
    ExportedObjectManager* object_manager,
    const dbus::ObjectPath& object_path,
    ProtocolHandler* protocol_handler,
    Server* server)
    : dbus_object_{new DBusObject{object_manager, object_manager->GetBus(),
                                  object_path}},
      protocol_handler_{protocol_handler},
      server_{server} {
  dbus_adaptor_.SetId(protocol_handler->GetID());
  dbus_adaptor_.SetName(protocol_handler->GetName());
  dbus_adaptor_.SetPort(protocol_handler->GetPort());
  dbus_adaptor_.SetProtocol(protocol_handler->GetProtocol());
  dbus_adaptor_.SetCertificateFingerprint(
      protocol_handler->GetCertificateFingerprint());
}

DBusProtocolHandler::~DBusProtocolHandler() {
  for (const auto& pair : dbus_service_data_) {
    server_->GetBus()->UnlistenForServiceOwnerChange(
        pair.first, pair.second.on_client_disconnected_callback);
  }
}

void DBusProtocolHandler::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  dbus_adaptor_.RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Failed exporting ProtocolHandler.", true));
  sequencer->OnAllTasksCompletedCall({completion_callback});
}

ExportedObjectManager* DBusProtocolHandler::GetObjectManager() const {
  return dbus_object_->GetObjectManager().get();
}

std::string DBusProtocolHandler::AddRequestHandler(
    const std::string& in_url,
    const std::string& in_method,
    const std::string& in_service_name) {
  auto p = dbus_service_data_.find(in_service_name);
  if (p == dbus_service_data_.end()) {
    DBusServiceData dbus_service_data;
    dbus_service_data.handler_proxy.reset(
        new RequestHandlerProxy{server_->GetBus(), in_service_name});
    dbus_service_data.on_client_disconnected_callback =
        base::Bind(&DBusProtocolHandler::OnClientDisconnected,
                   weak_ptr_factory_.GetWeakPtr(),
                   in_service_name);
    server_->GetBus()->ListenForServiceOwnerChange(
        in_service_name, dbus_service_data.on_client_disconnected_callback);
    p = dbus_service_data_.emplace(in_service_name,
                                   std::move(dbus_service_data)).first;
  }
  std::unique_ptr<RequestHandlerInterface> handler{
    new DBusRequestHandler{server_, p->second.handler_proxy.get()}
  };
  std::string handler_id = protocol_handler_->AddRequestHandler(
      in_url, in_method, std::move(handler));
  p->second.handler_ids.insert(handler_id);
  handler_to_service_name_map_.emplace(handler_id, in_service_name);

  return handler_id;
}

bool DBusProtocolHandler::RemoveRequestHandler(
    chromeos::ErrorPtr* error,
    const std::string& in_handler_id) {

  auto p = handler_to_service_name_map_.find(in_handler_id);
  if (p == handler_to_service_name_map_.end()) {
    chromeos::Error::AddToPrintf(error,
                                 FROM_HERE,
                                 chromeos::errors::dbus::kDomain,
                                 DBUS_ERROR_FAILED,
                                 "Handler with ID %s does not exist",
                                 in_handler_id.c_str());
    return false;
  }
  std::string service_name = p->second;
  CHECK(protocol_handler_->RemoveRequestHandler(in_handler_id));
  DBusServiceData& dbus_service_data = dbus_service_data_[service_name];
  CHECK_EQ(1u, dbus_service_data.handler_ids.erase(in_handler_id));
  if (dbus_service_data.handler_ids.empty()) {
    server_->GetBus()->UnlistenForServiceOwnerChange(
        service_name, dbus_service_data.on_client_disconnected_callback);
    dbus_service_data_.erase(service_name);
  }
  return true;
}

void DBusProtocolHandler::OnClientDisconnected(
    const std::string& service_name,
    const std::string& service_owner) {
  // This method will be called when the client's D-Bus service owner has
  // changed which could be either the client exiting (|service_owner| is empty)
  // or is being replaced with another running instance.
  // In either case, we need to remove the old client's handlers since the
  // new client will register their own on start up anyway.
  auto p = dbus_service_data_.find(service_name);
  if (p == dbus_service_data_.end())
    return;

  for (const std::string& handler_id : p->second.handler_ids) {
    handler_to_service_name_map_.erase(handler_id);
    protocol_handler_->RemoveRequestHandler(handler_id);
  }
  server_->GetBus()->UnlistenForServiceOwnerChange(
      service_name, p->second.on_client_disconnected_callback);
  dbus_service_data_.erase(p);
}

bool DBusProtocolHandler::GetRequestFileData(
    chromeos::ErrorPtr* error,
    const std::string& in_request_id,
    int32_t in_file_id,
    std::vector<uint8_t>* out_contents) {
  auto request = GetRequest(in_request_id, error);
  if (!request)
    return false;

  if (request->GetFileData(in_file_id, out_contents))
    return true;

  chromeos::Error::AddToPrintf(error,
                               FROM_HERE,
                               chromeos::errors::dbus::kDomain,
                               DBUS_ERROR_FAILED,
                               "File with ID %d does not exist",
                               in_file_id);
  return false;
}

bool DBusProtocolHandler::CompleteRequest(
    chromeos::ErrorPtr* error,
    const std::string& in_request_id,
    int32_t in_status_code,
    const std::vector<std::tuple<std::string, std::string>>& in_headers,
    const std::vector<uint8_t>& in_data) {
  auto request = GetRequest(in_request_id, error);
  if (!request)
    return false;

  if (request->Complete(in_status_code, in_headers, in_data)) {
    return true;
  }
  chromeos::Error::AddTo(error, FROM_HERE, chromeos::errors::dbus::kDomain,
                         DBUS_ERROR_FAILED, "Response already received");
  return false;
}

Request* DBusProtocolHandler::GetRequest(const std::string& request_id,
                                         chromeos::ErrorPtr* error) {
  Request* request = protocol_handler_->GetRequest(request_id);
  if (!request) {
    chromeos::Error::AddToPrintf(error,
                                 FROM_HERE,
                                 chromeos::errors::dbus::kDomain,
                                 DBUS_ERROR_FAILED,
                                 "Unknown request ID: %s",
                                 request_id.c_str());
  }
  return request;
}

}  // namespace webservd
