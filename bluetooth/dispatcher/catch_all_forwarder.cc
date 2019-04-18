// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/catch_all_forwarder.h"

#include <memory>
#include <string>

#include <base/bind.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/scoped_dbus_error.h>

#include "bluetooth/dispatcher/dbus_util.h"

namespace {

constexpr char kRootPath[] = "/";

void SendResponse(scoped_refptr<dbus::Bus> bus,
                  std::unique_ptr<dbus::Response> response) {
  // |bus| is an ad-hoc client-specific Bus which is not guaranteed to be always
  // connected. So check whether it's still connected before sending the
  // response.
  if (bus->IsConnected())
    bus->Send(response->raw_message(), nullptr);
}

}  // namespace

namespace bluetooth {

CatchAllForwarder::CatchAllForwarder(const scoped_refptr<dbus::Bus>& from_bus,
                                     const scoped_refptr<dbus::Bus>& to_bus,
                                     const std::string& destination)
    : from_bus_(from_bus), to_bus_(to_bus), destination_(destination) {}

CatchAllForwarder::~CatchAllForwarder() {
  Shutdown();
}

void CatchAllForwarder::Init() {
  VLOG(1) << __func__;
  from_bus_->AssertOnDBusThread();
  dbus::ScopedDBusError error;
  DBusObjectPathVTable vtable = {};
  vtable.message_function = &CatchAllForwarder::HandleMessageThunk;
  bool success = from_bus_->TryRegisterFallback(dbus::ObjectPath(kRootPath),
                                                &vtable, this, error.get());
  if (!success)
    LOG(ERROR) << "Failed to register object path fallback";
}

void CatchAllForwarder::Shutdown() {
  if (from_bus_->IsConnected())
    from_bus_->UnregisterObjectPath(dbus::ObjectPath(kRootPath));
}

DBusHandlerResult CatchAllForwarder::HandleMessageThunk(
    DBusConnection* connection, DBusMessage* raw_message, void* user_data) {
  CatchAllForwarder* self = reinterpret_cast<CatchAllForwarder*>(user_data);
  return self->HandleMessage(connection, raw_message);
}

DBusHandlerResult CatchAllForwarder::HandleMessage(DBusConnection* connection,
                                                   DBusMessage* raw_message) {
  if (dbus_message_get_type(raw_message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  dbus_message_ref(raw_message);
  std::unique_ptr<dbus::MethodCall> method_call(
      dbus::MethodCall::FromRawMessage(raw_message));
  VLOG(1) << "received message " << method_call->GetInterface() << "."
          << method_call->GetMember() << " to object "
          << method_call->GetPath().value() << " from "
          << method_call->GetSender();
  DBusUtil::ForwardMethodCall(to_bus_, destination_, method_call.get(),
                              base::Bind(&SendResponse, from_bus_));
  return DBUS_HANDLER_RESULT_HANDLED;
}

}  // namespace bluetooth
