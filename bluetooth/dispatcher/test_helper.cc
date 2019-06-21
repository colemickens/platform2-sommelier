// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/test_helper.h"

#include <memory>

#include <dbus/message.h>

namespace bluetooth {

namespace {

constexpr int kTestSerial = 1000;

}  // namespace

void StubHandleMethod(const std::string& expected_interface_name,
                      const std::string& expected_method_name,
                      const std::string& expected_payload,
                      const std::string& response_string,
                      const std::string& error_name,
                      const std::string& error_message,
                      dbus::MethodCall* method_call,
                      int timeout_ms,
                      dbus::ObjectProxy::ResponseCallback callback,
                      dbus::ObjectProxy::ErrorCallback error_callback) {
  // This stub doesn't handle method calls other than expected method.
  if (method_call->GetInterface() != expected_interface_name ||
      method_call->GetMember() != expected_method_name)
    return;

  dbus::MessageReader reader(method_call);
  std::string method_call_string;
  reader.PopString(&method_call_string);
  // This stub only accepts the expected test payload.
  if (method_call_string != expected_payload)
    return;

  method_call->SetSerial(kTestSerial);
  if (!error_name.empty()) {
    std::unique_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(method_call, error_name,
                                            error_message);
    error_callback.Run(error_response.get());
  } else {
    std::unique_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    writer.AppendString(response_string);
    callback.Run(response.get());
  }
}

}  // namespace bluetooth
