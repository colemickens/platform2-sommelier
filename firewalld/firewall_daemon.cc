// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/firewall_daemon.h"

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

#include <string>

#include <base/logging.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>

namespace {

const char kFirewallDaemonInterface[] = "org.chromium.FirewallDaemon";
const char kFirewallDaemonServicePath[] = "/org/chromium/FirewallDaemon";
const char kFirewallDaemonServiceName[] = "org.chromium.FirewallDaemon";

const char kPunchHole[] = "PunchHole";
const char kPlugHole[] = "PlugHole";

}

namespace firewalld {

FirewallDaemon::FirewallDaemon() {}

FirewallDaemon::~FirewallDaemon() {}

void FirewallDaemon::Run() {
  DBusConnection* const connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection) << "Cannot connect to system bus.";

  DBusError error;
  dbus_error_init(&error);
  dbus_bus_request_name(connection, kFirewallDaemonServiceName, 0, &error);
  if (dbus_error_is_set(&error)) {
    LOG(FATAL) << "Failed to register " << kFirewallDaemonServiceName << ": "
               << error.message;
    dbus_error_free(&error);
    return;
  }

  DBusObjectPathVTable vtable;
  memset(&vtable, 0, sizeof(vtable));
  vtable.message_function = &MainDBusMethodHandler;

  const dbus_bool_t registration_result = dbus_connection_register_object_path(
      connection, kFirewallDaemonServicePath, &vtable, this);
  CHECK(registration_result) << "Could not register object path.";

  GMainLoop* const loop = g_main_loop_new(NULL, false);
  g_main_loop_run(loop);
}

DBusHandlerResult FirewallDaemon::MainDBusMethodHandler(
    DBusConnection* connection,
    DBusMessage* message,
    void* data) {
  CHECK(connection) << "Missing connection.";
  CHECK(message) << "Missing method.";
  CHECK(data) << "Missing pointer to broker.";

  if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  std::string interface(dbus_message_get_interface(message));
  if (interface != kFirewallDaemonInterface)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  DBusMessage* reply = NULL;
  std::string member(dbus_message_get_member(message));
  FirewallDaemon* const broker = static_cast<FirewallDaemon*>(data);
  if (member == kPunchHole)
    reply = broker->HandlePunchHoleMethod(message);
  else if (member == kPlugHole)
    reply = broker->HandlePlugHoleMethod(message);
  else
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  CHECK(dbus_connection_send(connection, reply, NULL));
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusMessage* FirewallDaemon::HandlePunchHoleMethod(DBusMessage* message) {
  DBusMessage* reply = dbus_message_new_method_return(message);
  CHECK(reply) << "Could not allocate reply message for method call.";

  dbus_bool_t success = false;
  uint16_t port = 0;

  DBusError error;
  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_UINT16, &port,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << "Error parsing arguments: " << error.message;
    dbus_error_free(&error);

    dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &success,
                             DBUS_TYPE_INVALID);
    return reply;
  }

  if (port == 0) {
    // Port 0 is not a valid TCP/UDP port.
    success = false;
    dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &success,
                             DBUS_TYPE_INVALID);
    return reply;
  }

  // TODO(jorgelo): implement this.
  LOG(ERROR) << "Punching hole for port " << port;

  dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &success,
                           DBUS_TYPE_INVALID);
  return reply;
}

DBusMessage* FirewallDaemon::HandlePlugHoleMethod(DBusMessage* message) {
  DBusMessage* reply = dbus_message_new_method_return(message);
  CHECK(reply) << "Could not allocate reply message for method call.";

  dbus_bool_t success = false;
  uint16_t port = 0;

  DBusError error;
  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_UINT16, &port,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << "Error parsing arguments: " << error.message;
    dbus_error_free(&error);

    dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &success,
                             DBUS_TYPE_INVALID);
    return reply;
  }

  if (port == 0) {
    // Port 0 is not a valid TCP/UDP port.
    success = false;
    dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &success,
                             DBUS_TYPE_INVALID);
    return reply;
  }

  // TODO(jorgelo): implement this.
  LOG(ERROR) << "Plugging hole for port " << port;

  dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &success,
                           DBUS_TYPE_INVALID);
  return reply;
}

}  // namespace firewalld
