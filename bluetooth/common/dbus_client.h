// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_COMMON_DBUS_CLIENT_H_
#define BLUETOOTH_COMMON_DBUS_CLIENT_H_

#include <string>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>

namespace bluetooth {

// Represents a D-Bus client.
class DBusClient {
 public:
  DBusClient(const scoped_refptr<dbus::Bus>& bus,
             const std::string& client_address);
  ~DBusClient();

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

  // D-Bus address of this client.
  std::string client_address_;
  // The D-Bus match rule that has been registered to D-Bus daemon.
  std::string client_availability_match_rule_;

  // Callback to call when the client becomes unavailable.
  base::Closure client_unavailable_callback_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<DBusClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DBusClient);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_COMMON_DBUS_CLIENT_H_
