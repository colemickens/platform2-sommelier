// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_DBUS_HANDLER_H_
#define POWER_MANAGER_COMMON_DBUS_HANDLER_H_

#include <dbus/dbus-glib.h>

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

struct DBusMessage;
struct DBusConnection;

namespace chromeos {
namespace dbus {
class Proxy;
}  // namespace dbus
}  // namespace chromeos

namespace power_manager {
namespace util {

// This class dispatches messages received via D-Bus.
class DBusHandler {
 public:
  typedef base::Callback<bool(DBusMessage*)> SignalHandler;
  typedef base::Callback<DBusMessage*(DBusMessage*)> MethodHandler;
  typedef base::Callback<void(const std::string& /* name */,
                              const std::string& /* old_owner */,
                              const std::string& /* new_owner */)>
      NameOwnerChangedHandler;

  DBusHandler();
  ~DBusHandler();

  // Registers a handler for D-Bus signals or method calls named |member|
  // on |interface|.
  void AddSignalHandler(const std::string& interface,
                        const std::string& member,
                        const SignalHandler& handler);
  void AddMethodHandler(const std::string& interface,
                        const std::string& member,
                        const MethodHandler& handler);

  // Sets a callback for handling NamedOwnerChanged signals (emitted when a
  // D-Bus client connects or disconnects from the bus).
  void SetNameOwnerChangedHandler(const NameOwnerChangedHandler& callback);

  void Start();

 private:
  typedef std::pair<std::string, std::string> InterfaceMemberPair;
  typedef std::map<InterfaceMemberPair, SignalHandler> SignalHandlerTable;
  typedef std::map<InterfaceMemberPair, MethodHandler> MethodHandlerTable;

  // Handles a signal or method call being received.
  static DBusHandlerResult HandleMessageThunk(DBusConnection* connection,
                                              DBusMessage* message,
                                              void* data) {
    return static_cast<DBusHandler*>(data)->HandleMessage(connection, message);
  }
  DBusHandlerResult HandleMessage(DBusConnection* connection,
                                  DBusMessage* message);

  // Calls |name_owner_changed_handler_| in response to a name's owner
  // having changed.
  static void HandleNameOwnerChangedThunk(DBusGProxy*,
                                          const gchar* name,
                                          const gchar* old_owner,
                                          const gchar* new_owner,
                                          void* data) {
    static_cast<DBusHandler*>(data)->HandleNameOwnerChanged(
        name, old_owner, new_owner);
  }
  void HandleNameOwnerChanged(const gchar* name,
                              const gchar* old_owner,
                              const gchar* new_owner);

  // Used to listen for NameOwnerChanged signals.
  scoped_ptr<chromeos::dbus::Proxy> proxy_;

  SignalHandlerTable signal_handler_table_;
  MethodHandlerTable method_handler_table_;

  NameOwnerChangedHandler name_owner_changed_handler_;

  DISALLOW_COPY_AND_ASSIGN(DBusHandler);
};

}  // namespace util
}  // namespace power_manager

#endif //  POWER_MANAGER_COMMON_DBUS_HANDLER_H_
