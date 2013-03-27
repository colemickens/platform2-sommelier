// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/dbus_handler.h"

#include <dbus/dbus-glib-lowlevel.h>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/util_dbus.h"

namespace power_manager {
namespace util {

namespace {

// Adds a signal match rule to a D-Bus connection.
void AddSignalMatch(DBusConnection* connection,
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

}  // namespace

DBusHandler::DBusHandler() {}

DBusHandler::~DBusHandler() {}

void DBusHandler::AddSignalHandler(const std::string& interface,
                                   const std::string& member,
                                   const SignalHandler& handler) {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  AddSignalMatch(connection, interface, member);
  signal_handler_table_[std::make_pair(interface, member)] = handler;
}

void DBusHandler::AddMethodHandler(const std::string& interface,
                                   const std::string& member,
                                   const MethodHandler& handler) {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  method_handler_table_[std::make_pair(interface, member)] = handler;
}

void DBusHandler::SetNameOwnerChangedHandler(
    const NameOwnerChangedHandler& handler) {
  name_owner_changed_handler_ = handler;
}

void DBusHandler::Start() {
  const char kNameOwnerChangedSignal[] = "NameOwnerChanged";
  proxy_.reset(new chromeos::dbus::Proxy(
      chromeos::dbus::GetSystemBusConnection(),
      DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS));
  dbus_g_proxy_add_signal(proxy_->gproxy(), kNameOwnerChangedSignal,
                          G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                          G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(proxy_->gproxy(), kNameOwnerChangedSignal,
                              G_CALLBACK(HandleNameOwnerChangedThunk), this,
                              NULL);

  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);

  CHECK(dbus_connection_add_filter(
      connection, &HandleMessageThunk, this, NULL));

  DBusObjectPathVTable vtable;
  memset(&vtable, 0, sizeof(vtable));
  vtable.message_function = &HandleMessageThunk;
  CHECK(dbus_connection_register_object_path(
      connection, kPowerManagerServicePath, &vtable, this));

  LOG(INFO) << "D-Bus monitoring started";
}

DBusHandlerResult DBusHandler::HandleMessage(DBusConnection* connection,
                                             DBusMessage* message) {
  const char* interface = dbus_message_get_interface(message);
  const char* member = dbus_message_get_member(message);
  if (!interface || !member)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  switch (dbus_message_get_type(message)) {
    case DBUS_MESSAGE_TYPE_SIGNAL: {
      SignalHandlerTable::const_iterator it = signal_handler_table_.find(
          std::make_pair(std::string(interface), std::string(member)));
      if (it == signal_handler_table_.end())
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

      VLOG(1) << "Got " << member << " signal";
      return it->second.Run(message) ? DBUS_HANDLER_RESULT_HANDLED :
          DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    case DBUS_MESSAGE_TYPE_METHOD_CALL: {
      MethodHandlerTable::const_iterator it = method_handler_table_.find(
          std::make_pair(std::string(interface), std::string(member)));
      if (it == method_handler_table_.end()) {
        LOG(ERROR) << "Could not find handler for " << interface << ":"
                   << member << " in method handler table";
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
      }

      VLOG(1) << "Got " << member << " method call";
      DBusMessage* reply = it->second.Run(message);
      if (!reply)
        reply = util::CreateEmptyDBusReply(message);
      CHECK(dbus_connection_send(connection, reply, NULL));
      dbus_message_unref(reply);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    default:
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
}

void DBusHandler::HandleNameOwnerChanged(const gchar* name,
                                         const gchar* old_owner,
                                         const gchar* new_owner) {
  if (name && name[0] && !name_owner_changed_handler_.is_null()) {
    name_owner_changed_handler_.Run(
        name, old_owner ? old_owner : "", new_owner ? new_owner : "");
  }
}

}  // namespace util
}  // namespace power_manager
