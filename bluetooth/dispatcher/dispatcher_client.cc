// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/dispatcher_client.h"

#include <memory>

namespace bluetooth {

DispatcherClient::DispatcherClient(
    const scoped_refptr<dbus::Bus>& bus,
    const std::string& client_address,
    DBusConnectionFactory* dbus_connection_factory)
    : bus_(bus),
      dbus_connection_factory_(dbus_connection_factory),
      client_address_(client_address),
      dbus_client_(std::make_unique<DBusClient>(bus, client_address)),
      weak_ptr_factory_(this) {}

DispatcherClient::~DispatcherClient() {
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

  LOG(INFO) << "D-Bus connection name for client " << client_address_ << " = "
            << client_bus_->GetConnectionName();

  return client_bus_;
}

void DispatcherClient::StartUpwardForwarding() {
  if (catch_all_forwarder_)
    return;

  catch_all_forwarder_ = std::make_unique<CatchAllForwarder>(
      GetClientBus(), bus_, client_address_);
  catch_all_forwarder_->Init();
}

DBusClient* DispatcherClient::GetDBusClient() {
  return dbus_client_.get();
}

}  // namespace bluetooth
