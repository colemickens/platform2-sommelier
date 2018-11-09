// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/dispatcher_client.h"

#include <memory>
#include <string>

#include <base/location.h>
#include <base/strings/stringprintf.h>
#include <base/task_runner.h>
#include <dbus/message.h>
#include <dbus/scoped_dbus_error.h>

namespace {

// D-Bus constants for subscribing to NameOwnerChanged signals.
constexpr char kDBusSystemObjectPath[] = "/org/freedesktop/DBus";
constexpr char kDBusSystemObjectInterface[] = "org.freedesktop.DBus";
constexpr char kDBusSystemObjectAddress[] = "org.freedesktop.DBus";
constexpr char kNameOwnerChangedMember[] = "NameOwnerChanged";

}  // namespace

namespace bluetooth {

DispatcherClient::DispatcherClient(
    const scoped_refptr<dbus::Bus>& bus,
    const std::string& client_address,
    DBusConnectionFactory* dbus_connection_factory)
    : bus_(bus),
      dbus_connection_factory_(dbus_connection_factory),
      client_address_(client_address),
      weak_ptr_factory_(this) {}

DispatcherClient::~DispatcherClient() {
  // Clean up the match rule that has been added before.
  if (!client_availability_match_rule_.empty()) {
    dbus::ScopedDBusError error;
    bus_->RemoveMatch(client_availability_match_rule_, error.get());
    if (error.is_set())
      LOG(ERROR) << "Failed to remove match rule \""
                 << client_availability_match_rule_ << "\". Got "
                 << error.name() << ": " << error.message();
    client_availability_match_rule_.clear();
  }

  // Stop listening for any messages from D-Bus daemon.
  if (!client_unavailable_callback_.is_null())
    bus_->RemoveFilterFunction(&DispatcherClient::HandleMessageThunk, this);

  // Close the connection before this object is destructed.
  if (client_bus_)
    client_bus_->ShutdownAndBlock();
}

scoped_refptr<dbus::Bus> DispatcherClient::GetClientBus() {
  if (client_bus_)
    return client_bus_;

  VLOG(1) << "Creating a new D-Bus connection for client " << client_address_;

  client_bus_ = dbus_connection_factory_->GetNewBus();

  if (!client_bus_->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus for client "
               << client_address_;
    client_bus_ = nullptr;
    return nullptr;
  }

  VLOG(1) << "D-Bus connection name for client " << client_address_ << " = "
          << client_bus_->GetConnectionName();

  return client_bus_;
}

void DispatcherClient::WatchClientUnavailable(
    const base::Closure& client_unavailable_callback) {
  CHECK(client_unavailable_callback_.is_null())
      << "Client watch has been added before";

  client_unavailable_callback_ = client_unavailable_callback;

  // Listen to messages from D-Bus daemon.
  bus_->AddFilterFunction(&DispatcherClient::HandleMessageThunk, this);

  // We are only interested in the signal about the client becoming unavailable.
  // Add a filter here to be notified only for signals about the client's
  // NameOwnerChanged events.
  client_availability_match_rule_ = base::StringPrintf(
      "type='signal',interface='%s',member='%s',path='%s',sender='%s',"
      "arg0='%s'",
      kDBusSystemObjectInterface, kNameOwnerChangedMember,
      kDBusSystemObjectPath, kDBusSystemObjectAddress, client_address_.c_str());
  dbus::ScopedDBusError error;
  bus_->AddMatch(client_availability_match_rule_, error.get());
  if (error.is_set())
    LOG(ERROR) << "Failed to add match rule \""
               << client_availability_match_rule_ << "\". Got " << error.name()
               << ": " << error.message();
}

void DispatcherClient::StartUpwardForwarding() {
  if (catch_all_forwarder_)
    return;

  catch_all_forwarder_ = std::make_unique<CatchAllForwarder>(
      GetClientBus(), bus_, client_address_);
  catch_all_forwarder_->Init();
}

DBusHandlerResult DispatcherClient::HandleMessageThunk(
    DBusConnection* connection, DBusMessage* raw_message, void* user_data) {
  DispatcherClient* self = reinterpret_cast<DispatcherClient*>(user_data);
  CHECK(self);
  return self->HandleMessage(connection, raw_message);
}

DBusHandlerResult DispatcherClient::HandleMessage(DBusConnection* connection,
                                                  DBusMessage* raw_message) {
  if (dbus_message_get_type(raw_message) != DBUS_MESSAGE_TYPE_SIGNAL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  // raw_message will be unrefed on exit of the function. Increment the
  // reference so we can use it in |signal|.
  // TODO(sonnysasaka): libchrome should provide an API so we can avoid using
  // direct libdbus call dbus_message_ref().
  dbus_message_ref(raw_message);
  std::unique_ptr<dbus::Signal> signal(
      dbus::Signal::FromRawMessage(raw_message));

  // Confirm the validity of the NameOwnerChanged signal.
  if (signal->GetPath().value() == kDBusSystemObjectPath &&
      signal->GetMember() == kNameOwnerChangedMember &&
      signal->GetInterface() == kDBusSystemObjectInterface &&
      signal->GetSender() == kDBusSystemObjectAddress) {
    dbus::MessageReader reader(signal.get());
    std::string address, old_owner, new_owner;
    // A client is considered disconnected if |new_owner| is empty.
    if (reader.PopString(&address) && reader.PopString(&old_owner) &&
        reader.PopString(&new_owner) && address == client_address_ &&
        old_owner == client_address_ && new_owner.empty())
      bus_->GetOriginTaskRunner()->PostTask(FROM_HERE,
                                            client_unavailable_callback_);
  }

  // Always return unhandled to let other handlers handle the same signal.
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

}  // namespace bluetooth
