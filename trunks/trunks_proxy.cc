// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/trunks_proxy.h"

#include <base/bind.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/dbus/dbus_method_invoker.h>

#include "trunks/dbus_interface.h"
#include "trunks/dbus_interface.pb.h"
#include "trunks/error_codes.h"

namespace {

// Use a five minute timeout because some commands on some TPM hardware can take
// a very long time. If a few lengthy operations are already in the queue, a
// subsequent command needs to wait for all of them. Timeouts are always
// possible but under normal conditions 5 minutes seems to be plenty.
const int kDBusMaxTimeout = 5 * 60 * 1000;

}  // namespace

namespace trunks {

TrunksProxy::TrunksProxy() : weak_factory_(this) {}

TrunksProxy::~TrunksProxy() {
  if (bus_) {
    bus_->ShutdownAndBlock();
  }
}

bool TrunksProxy::Init() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  object_proxy_ = bus_->GetObjectProxy(
      trunks::kTrunksServiceName,
      dbus::ObjectPath(trunks::kTrunksServicePath));
  origin_thread_id_ = base::PlatformThread::CurrentId();
  return (object_proxy_ != nullptr);
}

void TrunksProxy::SendCommand(const std::string& command,
                              const ResponseCallback& callback) {
  if ((origin_thread_id_ != base::PlatformThread::CurrentId()) &&
      (!Init())) {
    LOG(ERROR) << "Error intializing trunks dbus proxy object.";
    callback.Run(CreateErrorResponse(TRUNKS_RC_IPC_ERROR));
  }
  SendCommandRequest tpm_command_proto;
  tpm_command_proto.set_command(command);
  auto on_error = [callback](chromeos::Error* error) {
    SendCommandResponse response;
    response.set_response(CreateErrorResponse(SAPI_RC_NO_RESPONSE_RECEIVED));
    callback.Run(response.response());
  };
  chromeos::dbus_utils::CallMethodWithTimeout(
      kDBusMaxTimeout,
      object_proxy_,
      trunks::kTrunksInterface,
      trunks::kSendCommand,
      callback,
      base::Bind(on_error),
      tpm_command_proto);
}

std::string TrunksProxy::SendCommandAndWait(const std::string& command) {
  if ((origin_thread_id_ != base::PlatformThread::CurrentId()) &&
      (!Init())) {
    LOG(ERROR) << "Error intializing trunks dbus proxy object.";
    return CreateErrorResponse(TRUNKS_RC_IPC_ERROR);
  }
  SendCommandRequest tpm_command_proto;
  tpm_command_proto.set_command(command);
  chromeos::ErrorPtr error;
  std::unique_ptr<dbus::Response> dbus_response =
      chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
          kDBusMaxTimeout,
          object_proxy_,
          trunks::kTrunksInterface,
          trunks::kSendCommand,
          &error,
          tpm_command_proto);
  SendCommandResponse tpm_response_proto;
  if (dbus_response.get() && chromeos::dbus_utils::ExtractMethodCallResults(
      dbus_response.get(), &error, &tpm_response_proto)) {
    return tpm_response_proto.response();
  } else {
    LOG(ERROR) << "TrunksProxy could not parse response: "
               << error->GetMessage();
    return CreateErrorResponse(SAPI_RC_MALFORMED_RESPONSE);
  }
}


}  // namespace trunks
