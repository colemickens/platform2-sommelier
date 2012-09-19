// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/util_dbus_handler.h"

#include <dbus/dbus-glib-lowlevel.h>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/util_dbus.h"

namespace power_manager {
namespace util {

DBusHandler::DBusHandler() {}

void DBusHandler::AddDBusSignalHandler(const std::string& interface,
                                       const std::string& member,
                                       const DBusSignalHandler& handler) {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  AddDBusSignalMatch(connection, interface, member);
  dbus_signal_handler_table_[std::make_pair(interface, member)] = handler;
}

void DBusHandler::AddDBusMethodHandler(const std::string& interface,
                                       const std::string& member,
                                       const DBusMethodHandler& handler) {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  dbus_method_handler_table_[std::make_pair(interface, member)] = handler;
}

void DBusHandler::Start() {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);

  CHECK(dbus_connection_add_filter(
      connection, &MainDBusSignalHandler, this, NULL));

  DBusObjectPathVTable vtable;
  memset(&vtable, 0, sizeof(vtable));
  vtable.message_function = &MainDBusMethodHandler;
  CHECK(dbus_connection_register_object_path(connection,
                                             kPowerManagerServicePath,
                                             &vtable,
                                             this));

  LOG(INFO) << "DBus monitoring started";
}

DBusHandlerResult DBusHandler::MainDBusSignalHandler(DBusConnection* connection,
                                                     DBusMessage* message,
                                                     void* data) {
  if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  DBusHandler* handler = static_cast<DBusHandler*>(data);
  CHECK(handler);
  // Look up and call the corresponding dbus message handler.
  std::string interface = dbus_message_get_interface(message);
  std::string member = dbus_message_get_member(message);
  DBusInterfaceMemberPair dbus_message_pair = std::make_pair(interface, member);
  DBusSignalHandlerTable::iterator iter =
      handler->dbus_signal_handler_table_.find(dbus_message_pair);
  // Quietly skip this signal if it has no handler.
  if (iter == handler->dbus_signal_handler_table_.end())
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  LOG(INFO) << "Got " << member << " signal";
  const DBusSignalHandler& callback = iter->second;

  if (callback.Run(message))
    return DBUS_HANDLER_RESULT_HANDLED;
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult DBusHandler::MainDBusMethodHandler(DBusConnection* connection,
                                                     DBusMessage* message,
                                                     void* data) {
  if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  DBusHandler* handler = static_cast<DBusHandler*>(data);
  CHECK(handler);
  // Look up and call the corresponding dbus message handler.
  std::string interface = dbus_message_get_interface(message);
  std::string member = dbus_message_get_member(message);
  DBusInterfaceMemberPair dbus_message_pair = std::make_pair(interface, member);
  DBusMethodHandlerTable::iterator iter =
      handler->dbus_method_handler_table_.find(dbus_message_pair);
  if (iter == handler->dbus_method_handler_table_.end()) {
    LOG(ERROR) << "Could not find handler for " << interface << ":" << member
               << " in method handler table.";
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  LOG(INFO) << "Got " << member << " method call";
  const DBusMethodHandler& callback = iter->second;
  DBusMessage* reply = callback.Run(message);

  // Must send a reply if it is a message.
  if (!reply)
    reply = util::CreateEmptyDBusReply(message);
  // If the send reply fails, it is due to lack of memory.
  CHECK(dbus_connection_send(connection, reply, NULL));
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

void DBusHandler::AddDBusSignalMatch(DBusConnection* connection,
                                     const std::string& interface,
                                     const std::string& member) {
  std::string filter;
  filter = base::StringPrintf("type='signal', interface='%s', member='%s'",
                        interface.c_str(), member.c_str());
  DBusError error;
  dbus_error_init(&error);
  dbus_bus_add_match(connection, filter.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to add a match: " << error.name << ", message="
               << error.message;
  }
}

}  // namespace util
}  // namespace power_manager
