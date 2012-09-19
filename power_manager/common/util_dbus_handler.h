// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_UTIL_DBUS_HANDLER_H_
#define POWER_MANAGER_COMMON_UTIL_DBUS_HANDLER_H_

#include <dbus/dbus-glib.h>

#include <map>
#include <string>

#include "base/callback.h"

struct DBusMessage;
struct DBusConnection;

namespace power_manager {
namespace util {

class DBusHandler {
 public:
  DBusHandler();

  typedef base::Callback<bool(DBusMessage*)> DBusSignalHandler;
  typedef base::Callback<DBusMessage*(DBusMessage*)> DBusMethodHandler;

  void AddDBusSignalHandler(const std::string& interface,
                            const std::string& member,
                            const DBusSignalHandler& handler);
  void AddDBusMethodHandler(const std::string& interface,
                            const std::string& member,
                            const DBusMethodHandler& handler);

  void Start();

 private:
  typedef std::pair<std::string, std::string> DBusInterfaceMemberPair;
  typedef std::map<DBusInterfaceMemberPair, DBusSignalHandler>
      DBusSignalHandlerTable;
  typedef std::map<DBusInterfaceMemberPair, DBusMethodHandler>
      DBusMethodHandlerTable;

  static DBusHandlerResult MainDBusMethodHandler(
      DBusConnection*, DBusMessage* message, void* data);
  static DBusHandlerResult MainDBusSignalHandler(
      DBusConnection*, DBusMessage* message, void* data);

  // Adds a signal match rule to a dbus connection.
  void AddDBusSignalMatch(DBusConnection* connection,
                          const std::string& interface,
                          const std::string& member);

  // These are lookup tables that map dbus message interface/names to handlers.
  DBusSignalHandlerTable dbus_signal_handler_table_;
  DBusMethodHandlerTable dbus_method_handler_table_;
};

}  // namespace util
}  // namespace power_manager

#endif //  POWER_MANAGER_COMMON_UTIL_DBUS_HANDLER_H_
