// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_DISPATCHER_CLIENT_H_
#define BLUETOOTH_DISPATCHER_DISPATCHER_CLIENT_H_

#include <memory>
#include <string>

#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>

#include "bluetooth/common/dbus_client.h"
#include "bluetooth/dispatcher/catch_all_forwarder.h"
#include "bluetooth/dispatcher/dbus_connection_factory.h"

namespace bluetooth {

// Represents a client of the Bluetooth dispatcher daemon.
class DispatcherClient {
 public:
  // |dbus_connection_factory| is not owned, must outlive this object.
  DispatcherClient(const scoped_refptr<dbus::Bus>& bus,
                   const std::string& client_address,
                   DBusConnectionFactory* dbus_connection_factory);
  ~DispatcherClient();

  // Returns the D-Bus connection to be used for forwarding messages to a
  // Bluetooth backend service (BlueZ/NewBlue).
  scoped_refptr<dbus::Bus> GetClientBus();

  // Starts "upward forwarding": forwarding method calls from server to client.
  void StartUpwardForwarding();

  // Returns the DBusClient.
  DBusClient* GetDBusClient();

 private:
  // The main D-Bus connection.
  scoped_refptr<dbus::Bus> bus_;
  // The D-Bus connection specific for message forwarding to Bluetooth service.
  scoped_refptr<dbus::Bus> client_bus_;

  DBusConnectionFactory* dbus_connection_factory_;

  // D-Bus address of this client.
  std::string client_address_;

  std::unique_ptr<DBusClient> dbus_client_;

  std::unique_ptr<CatchAllForwarder> catch_all_forwarder_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<DispatcherClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DispatcherClient);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_DISPATCHER_CLIENT_H_
