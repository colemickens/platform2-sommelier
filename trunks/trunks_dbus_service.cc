// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/trunks_dbus_service.h"

#include <utility>

#include <base/bind.h>

#include "trunks/dbus_interface.h"
#include "trunks/error_codes.h"
#include "trunks/interface.pb.h"

namespace trunks {

using brillo::dbus_utils::AsyncEventSequencer;
using brillo::dbus_utils::DBusMethodResponse;

TrunksDBusService::TrunksDBusService()
    : brillo::DBusServiceDaemon(trunks::kTrunksServiceName) {}

void TrunksDBusService::RegisterDBusObjectsAsync(
    AsyncEventSequencer* sequencer) {
  trunks_dbus_object_.reset(new brillo::dbus_utils::DBusObject(
      nullptr, bus_, dbus::ObjectPath(kTrunksServicePath)));
  brillo::dbus_utils::DBusInterface* dbus_interface =
      trunks_dbus_object_->AddOrGetInterface(kTrunksInterface);
  dbus_interface->AddMethodHandler(kSendCommand, base::Unretained(this),
                                   &TrunksDBusService::HandleSendCommand);
  trunks_dbus_object_->RegisterAsync(
      sequencer->GetHandler("Failed to register D-Bus object.", true));
  if (power_manager_) {
    power_manager_->Init(bus_);
  }
}

void TrunksDBusService::OnShutdown(int* exit_code) {
  if (power_manager_) {
    power_manager_->TearDown();
  }
  DBusServiceDaemon::OnShutdown(exit_code);
}

void TrunksDBusService::HandleSendCommand(
    std::unique_ptr<DBusMethodResponse<const SendCommandResponse&>>
        response_sender,
    const SendCommandRequest& request) {
  // Convert |response_sender| to a shared_ptr so |transceiver_| can safely
  // copy the callback.
  using SharedResponsePointer =
      std::shared_ptr<DBusMethodResponse<const SendCommandResponse&>>;
  // A callback that constructs the response protobuf and sends it.
  auto callback = [](const SharedResponsePointer& response,
                     const std::string& response_from_tpm) {
    SendCommandResponse tpm_response_proto;
    tpm_response_proto.set_response(response_from_tpm);
    response->Return(tpm_response_proto);
  };
  if (!request.has_command() || request.command().empty()) {
    LOG(ERROR) << "TrunksDBusService: Invalid request.";
    callback(SharedResponsePointer(std::move(response_sender)),
             CreateErrorResponse(SAPI_RC_BAD_PARAMETER));
    return;
  }
  transceiver_->SendCommand(
      request.command(),
      base::Bind(callback, SharedResponsePointer(std::move(response_sender))));
}

}  // namespace trunks
