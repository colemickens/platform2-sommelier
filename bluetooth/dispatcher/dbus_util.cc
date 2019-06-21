// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/dbus_util.h"

#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <dbus/object_proxy.h>

namespace {

// Called when the return of a forwarded message is received.
void OnMessageForwardResponse(
    uint32_t serial,
    const std::string& sender,
    bluetooth::DBusUtil::ResponseSender response_sender,
    dbus::Response* response) {
  // To forward the response back to the original client, we need to set the
  // D-Bus reply serial and destination fields after copying the response
  // message.
  // TODO(sonnysasaka): Avoid using libdbus' dbus_message_copy directly and
  // use dbus::Response::CopyMessage() when it's available in libchrome.
  std::unique_ptr<dbus::Response> response_copy(dbus::Response::FromRawMessage(
      dbus_message_copy(response->raw_message())));
  response_copy->SetReplySerial(serial);
  response_copy->SetDestination(sender);
  response_sender.Run(std::move(response_copy));
}

// Called when the error return of the forwarded message is received.
void OnMessageForwardError(uint32_t serial,
                           const std::string& sender,
                           bluetooth::DBusUtil::ResponseSender response_sender,
                           dbus::ErrorResponse* response) {
  // Same as OnMessageForwardResponse, but use
  // dbus::ErrorResponse::FromRawMessage because we know that |response| is a
  // D-Bus message of type METHOD_RETURN.
  std::unique_ptr<dbus::Response> response_copy(
      dbus::ErrorResponse::FromRawMessage(
          dbus_message_copy(response->raw_message())));
  response_copy->SetReplySerial(serial);
  response_copy->SetDestination(sender);
  response_sender.Run(std::move(response_copy));
}

}  // namespace

namespace bluetooth {

void DBusUtil::ForwardMethodCall(scoped_refptr<dbus::Bus> bus,
                                 const std::string& destination_service,
                                 dbus::MethodCall* method_call,
                                 ResponseSender response_sender) {
  // Here we forward a D-Bus message to another service.
  // After copying the message, we don't need to set destination/serial/sender
  // manually as this will be done by the lower level API already.
  std::unique_ptr<dbus::MethodCall> method_call_copy(
      dbus::MethodCall::FromRawMessage(
          dbus_message_copy(method_call->raw_message())));
  // TODO(sonnysasaka): Migrate to CallMethodWithErrorResponse after libchrome
  // is uprevved.
  bus->GetObjectProxy(destination_service, method_call->GetPath())
      ->CallMethodWithErrorCallback(
          method_call_copy.get(), dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
          base::Bind(&OnMessageForwardResponse, method_call->GetSerial(),
                     method_call->GetSender(), response_sender),
          base::Bind(&OnMessageForwardError, method_call->GetSerial(),
                     method_call->GetSender(), response_sender));
}

}  // namespace bluetooth
