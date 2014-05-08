// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/dbus_utils.h"

#include <base/logging.h>
#include <base/bind.h>

namespace buffet {

namespace dbus_utils {

namespace {

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns NULL, an empty response is created
// and sent.
void HandleSynchronousDBusMethodCall(
    base::Callback<scoped_ptr<dbus::Response>(dbus::MethodCall*)> handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  auto response = handler.Run(method_call);
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);

  response_sender.Run(response.Pass());
}

}  // namespace

scoped_ptr<dbus::Response> GetBadArgsError(dbus::MethodCall* method_call,
                                           const std::string& message) {
  LOG(ERROR) << "Error while handling DBus call: " << message;
  scoped_ptr<dbus::ErrorResponse> resp(dbus::ErrorResponse::FromMethodCall(
      method_call, "org.freedesktop.DBus.Error.InvalidArgs", message));
  return scoped_ptr<dbus::Response>(resp.release());
}

scoped_ptr<dbus::Response> GetDBusError(dbus::MethodCall* method_call,
                                        const chromeos::Error* error) {
  std::string message;
  while (error) {
    // Format error string as "domain/code:message".
    if (!message.empty())
      message += ';';
    message += error->GetDomain() + '/' + error->GetCode() + ':' +
               error->GetMessage();
    error = error->GetInnerError();
  }
  scoped_ptr<dbus::ErrorResponse> resp(dbus::ErrorResponse::FromMethodCall(
    method_call, "org.freedesktop.DBus.Error.Failed", message));
  return scoped_ptr<dbus::Response>(resp.release());
}

dbus::ExportedObject::MethodCallCallback GetExportableDBusMethod(
    base::Callback<scoped_ptr<dbus::Response>(dbus::MethodCall*)> handler) {
  return base::Bind(&HandleSynchronousDBusMethodCall, handler);
}

}  // namespace dbus_utils

}  // namespace buffet
