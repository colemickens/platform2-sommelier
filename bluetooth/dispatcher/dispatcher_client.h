// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_DISPATCHER_CLIENT_H_
#define BLUETOOTH_DISPATCHER_DISPATCHER_CLIENT_H_

#include <string>

#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>

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

  // Registers a listener to be notified when this client becomes unavailable
  // (disconnected from D-Bus). This should be called only once.
  void WatchClientUnavailable(const base::Closure& client_unavailable_callback);

 private:
  // Accepts matched messages from D-Bus daemon, relays it to |HandleMessage|.
  // This adapter is needed since libdbus only accepts static C function as
  // the callback.
  static DBusHandlerResult HandleMessageThunk(DBusConnection* connection,
                                              DBusMessage* raw_message,
                                              void* user_data);
  DBusHandlerResult HandleMessage(DBusConnection* connection,
                                  DBusMessage* raw_message);

  // The main D-Bus connection. Used for listening to NameOwnerChanged to detect
  // the client becoming unavailable.
  scoped_refptr<dbus::Bus> bus_;
  // The D-Bus connection specific for message forwarding to Bluetooth service.
  scoped_refptr<dbus::Bus> client_bus_;

  DBusConnectionFactory* dbus_connection_factory_;

  // D-Bus address of this client.
  std::string client_address_;
  // The D-Bus match rule that has been registered to D-Bus daemon.
  std::string client_availability_match_rule_;

  // Callback to call when the client becomes unavailable.
  base::Closure client_unavailable_callback_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<DispatcherClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DispatcherClient);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_DISPATCHER_CLIENT_H_
