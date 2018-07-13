// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_CLIENT_MANAGER_H_
#define BLUETOOTH_DISPATCHER_CLIENT_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include <base/memory/ref_counted.h>
#include <dbus/bus.h>

#include "bluetooth/dispatcher/dbus_connection_factory.h"
#include "bluetooth/dispatcher/dispatcher_client.h"

namespace bluetooth {

// Keeps track of clients of dispatcher. For each existing client, it keeps a
// separate dedicated D-Bus connection until the client is disconnected from
// the main D-Bus connection.
class ClientManager {
 public:
  ClientManager(scoped_refptr<dbus::Bus> bus,
                std::unique_ptr<DBusConnectionFactory> dbus_connection_factory);
  ~ClientManager() = default;

  // Adds a new DispatcherClient for address |client_address| if not yet added.
  DispatcherClient* EnsureClientAdded(const std::string& client_address);

 private:
  // Called when a client is disconnected from D-Bus.
  void OnClientUnavailable(const std::string& client_address);

  scoped_refptr<dbus::Bus> bus_;

  // Keeps which clients called the exposed methods. A client will be removed
  // from this list when it's disconnected from D-Bus.
  // It's a map of DispatcherClient objects indexed by their D-Bus addresses.
  std::map<std::string, std::unique_ptr<DispatcherClient>> clients_;

  std::unique_ptr<DBusConnectionFactory> dbus_connection_factory_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<ClientManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientManager);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_CLIENT_MANAGER_H_
