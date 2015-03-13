// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/trunks_proxy.h"

#include <base/bind.h>
#include <base/stl_util.h>

#include "trunks/dbus_interface.h"
#include "trunks/dbus_interface.pb.h"
#include "trunks/error_codes.h"

namespace {

// Use a five minute timeout because some commands on some TPM hardware can take
// a very long time. If a few lengthy operations are already in the queue, a
// subsequent command needs to wait for all of them. Timeouts are always
// possible but under normal conditions 5 minutes seems to be plenty.
const int kDbusMaxTimeout = 5 * 60 * 1000;

}  // namespace

namespace trunks {

TrunksProxy::TrunksProxy() : weak_factory_(this) {}

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
                              const ResponseCallback& callback) {
  scoped_ptr<dbus::MethodCall> method_call =
      CreateSendCommandMethodCall(command);
  object_->CallMethod(method_call.get(), kDbusMaxTimeout,
                      base::Bind(&trunks::TrunksProxy::OnResponse,
                                 GetWeakPtr(),
                                 callback));
}

std::string TrunksProxy::SendCommandAndWait(const std::string& command) {
  scoped_ptr<dbus::MethodCall> method_call =
      CreateSendCommandMethodCall(command);
  scoped_ptr<dbus::Response> dbus_response =
      object_->CallMethodAndBlock(method_call.get(), kDbusMaxTimeout);
  return GetResponseData(dbus_response.get());
}

void TrunksProxy::OnResponse(const ResponseCallback& callback,
                             dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << "TrunksProxy: No response!";
    callback.Run(CreateErrorResponse(
        SAPI_RC_NO_RESPONSE_RECEIVED));
    return;
  }
  callback.Run(GetResponseData(response));
}

std::string TrunksProxy::GetResponseData(dbus::Response* response) {
  dbus::MessageReader reader(response);
  SendCommandResponse tpm_response_proto;
  if (!reader.PopArrayOfBytesAsProto(&tpm_response_proto)) {
    LOG(ERROR) << "TrunksProxy was not able to parse the response.";
    return CreateErrorResponse(SAPI_RC_MALFORMED_RESPONSE);
  }
  return tpm_response_proto.response();
}

scoped_ptr<dbus::MethodCall> TrunksProxy::CreateSendCommandMethodCall(
    const std::string& command) {
  CHECK(!command.empty());
  scoped_ptr<dbus::MethodCall> method_call(new dbus::MethodCall(
      trunks::kTrunksInterface,
      trunks::kSendCommand));
  dbus::MessageWriter writer(method_call.get());
  SendCommandRequest tpm_command_proto;
  tpm_command_proto.set_command(command);
  writer.AppendProtoAsArrayOfBytes(tpm_command_proto);
  return method_call.Pass();
}

}  // namespace trunks
