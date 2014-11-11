// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/dbus/dbus_method_response.h>

#include <chromeos/dbus/utils.h>

namespace chromeos {
namespace dbus_utils {

DBusMethodResponse::DBusMethodResponse(dbus::MethodCall* method_call,
                                       ResponseSender sender)
    : sender_(sender),
      method_call_(method_call) {
}

DBusMethodResponse::~DBusMethodResponse() {
  if (method_call_) {
    // Response hasn't been sent by the handler. Abort the call.
    Abort();
  }
}

void DBusMethodResponse::ReplyWithError(const chromeos::Error* error) {
  CheckCanSendResponse();
  auto response = GetDBusError(method_call_, error);
  SendRawResponse(scoped_ptr<dbus::Response>(response.release()));
}

void DBusMethodResponse::ReplyWithError(
    const tracked_objects::Location& location,
    const std::string& error_domain,
    const std::string& error_code,
    const std::string& error_message) {
  ErrorPtr error;
  Error::AddTo(&error, location, error_domain, error_code, error_message);
  ReplyWithError(error.get());
}

void DBusMethodResponse::Abort() {
  SendRawResponse(scoped_ptr<dbus::Response>());
}

void DBusMethodResponse::SendRawResponse(scoped_ptr<dbus::Response> response) {
  CheckCanSendResponse();
  method_call_ = nullptr;  // Mark response as sent.
  sender_.Run(response.Pass());
}

scoped_ptr<dbus::Response> DBusMethodResponse::CreateCustomResponse() const {
  return dbus::Response::FromMethodCall(method_call_);
}

bool DBusMethodResponse::IsResponseSent() const {
  return (method_call_ == nullptr);
}

void DBusMethodResponse::CheckCanSendResponse() const {
  CHECK(method_call_) << "Response already sent";
}

}  // namespace dbus_utils
}  // namespace chromeos
