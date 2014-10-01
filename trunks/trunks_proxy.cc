// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>
#include <base/stl_util.h>

#include "trunks/dbus_interface.h"
#include "trunks/error_codes.h"
#include "trunks/tpm_communication.pb.h"
#include "trunks/trunks_proxy.h"

namespace trunks {

TrunksProxy::TrunksProxy() {}

TrunksProxy::~TrunksProxy() {}

bool TrunksProxy::Init() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  if (!bus_)
    return false;
  object_ = bus_->GetObjectProxy(
      trunks::kTrunksServiceName,
      dbus::ObjectPath(trunks::kTrunksServicePath));
  if (!object_)
    return false;
  return true;
}

void TrunksProxy::SendCommand(const std::string& command,
                              const SendCommandCallback& callback) {
  dbus::MethodCall method_call(trunks::kTrunksInterface, trunks::kSendCommand);
  dbus::MessageWriter writer(&method_call);
  CHECK(!command.empty());
  TpmCommand tpm_command_proto;
  tpm_command_proto.set_command(command);
  writer.AppendProtoAsArrayOfBytes(tpm_command_proto);
  object_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                      base::Bind(&trunks::TrunksProxy::OnResponse, callback));
}

uint32_t TrunksProxy::SendCommandAndWait(const std::string& command,
                                         std::string* response) {
  return 0;
}
void TrunksProxy::OnResponse(const SendCommandCallback& callback,
                             dbus::Response* response) {
  if (!response) {
    LOG(INFO) << "No response seen.";
    callback.Run("");
    return;
  }
  dbus::MessageReader reader(response);
  TpmResponse tpm_response_proto;
  if (!reader.PopArrayOfBytesAsProto(&tpm_response_proto)) {
    LOG(ERROR) << "TrunksProxy was not able to parse the response.";
    callback.Run(std::string());
    return;
  }
  if (tpm_response_proto.has_result_code()) {
    TPM_RC rc = tpm_response_proto.result_code();
    if (rc != TPM_RC_SUCCESS) {
      LOG(ERROR) << "Trunks Daemon returned an error code: " << rc;
    }
  }
  callback.Run(tpm_response_proto.response());
}

}  // namespace trunks

