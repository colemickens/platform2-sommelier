// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus/dbus-glib-lowlevel.h>
#include <gflags/gflags.h>
#include <time.h>

#include <iostream>

#include "power_manager/util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

using std::string;
using std::cout;
using std::endl;

unsigned int sequence_num = 0;

void RegisterSuspendDelay() {
  DBusGConnection* connection;
  GError* error = NULL;
  unsigned int delay_ms = 6000;
  connection = chromeos::dbus::GetSystemBusConnection().g_connection();
  CHECK(connection);
  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      connection,
      "org.chromium.PowerManager",
      "/",
      "org.chromium.PowerManager");
  CHECK(proxy);
  error = NULL;
  if (!dbus_g_proxy_call(proxy, "RegisterSuspendDelay",
                         &error, G_TYPE_UINT,
                         delay_ms, G_TYPE_INVALID, G_TYPE_INVALID)) {
    LOG(ERROR) << "Error Registering: " << error->message;
  }
  g_object_unref(proxy);
}

void UnregisterSuspendDelay() {
  DBusGConnection* connection;
  GError* error = NULL;
  connection = chromeos::dbus::GetSystemBusConnection().g_connection();
  CHECK(connection);
  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      connection,
      "org.chromium.PowerManager",
      "/",
      "org.chromium.PowerManager");
  CHECK(proxy);
  error = NULL;
  if (!dbus_g_proxy_call(proxy, "UnregisterSuspendDelay",
                         &error,
                         G_TYPE_INVALID, G_TYPE_INVALID)) {
    LOG(ERROR) << "Error Registering: " << error->message;
  }
  g_object_unref(proxy);
}
gboolean SendSuspendReady(gpointer) {
  const char* signal_name = "SuspendReady";
  dbus_uint32_t payload = sequence_num;
  LOG(INFO) << "Sending Broadcast '" << signal_name << "' to PowerManager:";
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              "/",
                              power_manager::kPowerManagerInterface);
  DBusMessage* signal = ::dbus_message_new_signal(
      "/",
      power_manager::kPowerManagerInterface,
      signal_name);
  CHECK(signal);
  dbus_message_append_args(signal, DBUS_TYPE_UINT32, &payload);
  ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  ::dbus_message_unref(signal);

  return false;
}

void SuspendDelaySignaled(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error, DBUS_TYPE_UINT32, &sequence_num,
                             DBUS_TYPE_INVALID)) {
    cout << "Could not get args from SuspendDelay signal!" << endl;
    if (dbus_error_is_set(&error))
      dbus_error_free(&error);
    return;
  }
  cout << "sequence num = " << sequence_num << endl;
  cout << "sleeping..." << endl;
  g_timeout_add(5000, SendSuspendReady, NULL);
}

DBusHandlerResult DBusMessageHandler(
    DBusConnection*, DBusMessage* message, void*) {
  cout << "[DBusMessageHandler] Sender : " << dbus_message_get_sender(message)
       << endl;
  if (dbus_message_is_signal(message, power_manager::kPowerManagerInterface,
                             power_manager::kSuspendDelay)) {
    cout << "Suspend Delayed event" << endl;
    SuspendDelaySignaled(message);
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

void signal_handler(DBusGProxy*,
                    const gchar* name,
                    const gchar* old_owner,
                    const gchar* new_owner,
                    gpointer) {
    cout << "name : " << name << endl;
    cout << "old_owner : " << old_owner << endl;
    cout << "new owner : " << new_owner << endl;
    if (0 == strlen(new_owner))
      cout << "BALEETED!" << endl;
}

void RegisterDBusMessageHandler() {
  const string filter = StringPrintf("type='signal', interface='%s'",
                                     power_manager::kPowerManagerInterface);
  DBusError error;
  dbus_error_init(&error);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  dbus_bus_add_match(connection, filter.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    cout << "Failed to add a filter:" << error.name << ", message="
               << error.message << endl;
    NOTREACHED();
  } else {
    CHECK(dbus_connection_add_filter(connection, &DBusMessageHandler, NULL,
                                     NULL));
    cout << "DBus monitoring started" << endl;
  }

  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      chromeos::dbus::GetSystemBusConnection().g_connection(),
      "org.freedesktop.DBus",
      "/org/freedesktop/DBus",
      "org.freedesktop.DBus");
  if (NULL == proxy) {
    cout << "Failed to connect to freedesktop dbus server." << endl;
    NOTREACHED();
  }
    dbus_g_proxy_add_signal(proxy, "NameOwnerChanged",
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(proxy, "NameOwnerChanged",
                                G_CALLBACK(signal_handler), NULL, NULL);
}

int main(int argc, char* argv[]) {
  GMainLoop* loop;
  g_type_init();
  google::ParseCommandLineFlags(&argc, &argv, true);
  loop = g_main_loop_new(NULL, false);
  cout << "Suspend Delay Test!" << endl;
  RegisterDBusMessageHandler();
  RegisterSuspendDelay();
  g_main_loop_run(loop);
  return 0;
}


