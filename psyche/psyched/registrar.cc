// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/registrar.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop/message_loop.h>
#include <libprotobinder/binder_manager.h>
#include <libprotobinder/binder_proxy.h>
#include <libprotobinder/iservice_manager.h>

#include "psyche/common/constants.h"
#include "psyche/common/util.h"
#include "psyche/psyched/client.h"
#include "psyche/psyched/service.h"

using protobinder::BinderManager;
using protobinder::BinderProxy;

namespace psyche {

Registrar::Registrar() : weak_ptr_factory_(this) {}

Registrar::~Registrar() = default;

int Registrar::RegisterService(RegisterServiceRequest* in,
                               RegisterServiceResponse* out) {
  const std::string service_name = in->name();
  scoped_ptr<BinderProxy> proxy =
      util::ExtractBinderProxyFromProto(in->mutable_binder());
  LOG(INFO) << "Got request to register \"" << service_name << "\" with "
            << "handle " << proxy->handle();

  if (service_name.empty()) {
    LOG(ERROR) << "Ignoring request to register service with invalid name";
    out->set_success(false);
    return 0;
  }

  auto it = services_.find(service_name);
  if (it == services_.end()) {
    it = services_.insert(std::make_pair(service_name,
        make_linked_ptr(new Service(service_name)))).first;
  } else {
    // The service is already registered (but maybe it's dead).
    if (it->second->state() == Service::STATE_STARTED) {
      LOG(ERROR) << "Ignoring request to register already-running service \""
                 << service_name << "\"";
      out->set_success(false);
      return 0;
    }
  }
  Service* service = it->second.get();
  service->SetProxy(proxy.Pass());

  out->set_success(true);
  return 0;
}

int Registrar::RequestService(RequestServiceRequest* in,
                              RequestServiceResponse* out) {
  const std::string service_name = in->name();
  scoped_ptr<BinderProxy> client_proxy =
      util::ExtractBinderProxyFromProto(in->mutable_client_binder());
  int32_t client_handle = client_proxy->handle();
  LOG(INFO) << "Got request to provide service \"" << service_name << "\""
            << " to client with handle " << client_handle;

  auto service_it = services_.find(service_name);
  if (service_it == services_.end()) {
    LOG(WARNING) << "Service is not registered";
    out->set_success(false);
    return 0;
  }

  auto client_it = clients_.find(client_handle);
  if (client_it == clients_.end()) {
    // We didn't already know about the client.
    client_proxy->SetDeathCallback(base::Bind(
        &Registrar::HandleClientBinderDeath,
        weak_ptr_factory_.GetWeakPtr(), client_handle));
    client_it = clients_.insert(std::make_pair(client_handle,
        make_linked_ptr(new Client(client_proxy.Pass())))).first;
  }
  Client* client = client_it->second.get();
  Service* service = service_it->second.get();

  // Check that the client didn't previously request this service.
  if (service->clients().count(client) == 0) {
    service->AddClient(client);
    client->AddService(service);
  }

  out->set_success(true);
  return 0;
}

void Registrar::HandleClientBinderDeath(int32_t handle) {
  LOG(INFO) << "Got binder death notification for client with handle "
            << handle;

  auto it = clients_.find(handle);
  if (it == clients_.end()) {
    LOG(ERROR) << "Ignoring death notification for unknown client with handle "
               << handle;
    return;
  }

  Client* client = it->second.get();
  for (auto service : client->services())
    service->RemoveClient(client);

  // TODO(derat): Stop unused services?

  clients_.erase(it);
}

}  // namespace psyche
