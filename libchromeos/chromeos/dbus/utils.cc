// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/dbus/utils.h>

#include <base/bind.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/any.h>
#include <chromeos/errors/error_codes.h>

namespace chromeos {
namespace dbus_utils {

namespace {

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns nullptr, an empty response is created
// and sent.
void HandleSynchronousDBusMethodCall(
    const MethodCallHandler& handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  scoped_ptr<dbus::Response> response(handler.Run(method_call).release());
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);

  response_sender.Run(response.Pass());
}

}  // namespace

std::unique_ptr<dbus::Response> CreateDBusErrorResponse(
    dbus::MethodCall* method_call,
    const std::string& code,
    const std::string& message) {
  auto resp = dbus::ErrorResponse::FromMethodCall(method_call, code, message);
  return std::unique_ptr<dbus::Response>(resp.release());
}

std::unique_ptr<dbus::Response> GetDBusError(dbus::MethodCall* method_call,
                                             const chromeos::Error* error) {
  std::string error_code = DBUS_ERROR_FAILED;  // Default error code.
  std::string error_message;

  // Special case for "dbus" error domain.
  // Pop the error code and message from the error chain and use them as the
  // actual D-Bus error message.
  if (error->GetDomain() == errors::dbus::kDomain) {
    error_code = error->GetCode();
    error_message = error->GetMessage();
    error = error->GetInnerError();
  }

  // Append any inner errors to the error message.
  while (error) {
    // Format error string as "domain/code:message".
    if (!error_message.empty())
      error_message += ';';
    error_message += error->GetDomain() + '/' + error->GetCode() + ':' +
               error->GetMessage();
    error = error->GetInnerError();
  }
  return CreateDBusErrorResponse(method_call, error_code, error_message);
}

dbus::ExportedObject::MethodCallCallback GetExportableDBusMethod(
    const MethodCallHandler& handler) {
  return base::Bind(&HandleSynchronousDBusMethodCall, handler);
}

}  // namespace dbus_utils
}  // namespace chromeos
