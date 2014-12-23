// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/trunks_service.h"

#include <base/bind.h>
#include <base/logging.h>
#include <base/stl_util.h>

#include "trunks/command_transceiver.h"
#include "trunks/dbus_interface.h"
#include "trunks/dbus_interface.pb.h"
#include "trunks/error_codes.h"
#include "trunks/tpm_utility_impl.h"

namespace trunks {

TrunksService::TrunksService(CommandTransceiver* transceiver)
    : bus_(nullptr),
      trunks_dbus_object_(nullptr),
      transceiver_(transceiver),
      weak_factory_(this) {}

TrunksService::~TrunksService() {}

void TrunksService::Init() {
  InitDBusService();
}

void TrunksService::HandleSendCommand(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  base::Callback<void(const std::string& response)> callback =
      base::Bind(&TrunksService::OnResponse,
                 GetWeakPtr(),
                 response_sender,
                 base::Passed(&response));
  dbus::MessageReader reader(method_call);
  SendCommandRequest tpm_command_proto;
  if (!reader.PopArrayOfBytesAsProto(&tpm_command_proto) ||
      !tpm_command_proto.has_command() ||
      tpm_command_proto.command().empty()) {
    LOG(ERROR) << "TrunksService: Invalid request.";
    callback.Run(TpmUtilityImpl::CreateErrorResponse(SAPI_RC_BAD_PARAMETER));
    return;
  }
  transceiver_->SendCommand(tpm_command_proto.command(), callback);
}

void TrunksService::OnResponse(
    dbus::ExportedObject::ResponseSender response_sender,
    scoped_ptr<dbus::Response> dbus_response,
    const std::string& response_from_tpm) {
  SendCommandResponse tpm_response_proto;
  tpm_response_proto.set_response(response_from_tpm);
  dbus::MessageWriter writer(dbus_response.get());
  writer.AppendProtoAsArrayOfBytes(tpm_response_proto);
  response_sender.Run(dbus_response.Pass());
}

void TrunksService::InitDBusService() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  CHECK(bus_->Connect());

  trunks_dbus_object_ = bus_->GetExportedObject(
      dbus::ObjectPath(kTrunksServicePath));
  CHECK(trunks_dbus_object_);

  trunks_dbus_object_->ExportMethodAndBlock(kTrunksInterface, kSendCommand,
      base::Bind(&TrunksService::HandleSendCommand, GetWeakPtr()));

  CHECK(bus_->RequestOwnershipAndBlock(kTrunksServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << kTrunksServiceName;
}

}  // namespace trunks
