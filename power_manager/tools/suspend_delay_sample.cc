// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus/dbus-glib-lowlevel.h>
#include <gflags/gflags.h>
#include <time.h>

#include <iostream>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/util_dbus.h"
#include "power_manager/common/util_dbus_handler.h"

using std::string;
using std::cout;
using std::endl;

unsigned int sequence_num = 0;

power_manager::util::DBusHandler dbus_handler;

void RegisterSuspendDelay() {
  GError* error = NULL;
  unsigned int delay_ms = 6000;
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              power_manager::kPowerManagerServiceName,
                              "/",
                              power_manager::kPowerManagerInterface);
  error = NULL;
  if (!dbus_g_proxy_call(proxy.gproxy(), "RegisterSuspendDelay",
                         &error, G_TYPE_UINT,
                         delay_ms, G_TYPE_INVALID, G_TYPE_INVALID)) {
    LOG(ERROR) << "Error Registering: " << error->message;
  }
}

void UnregisterSuspendDelay() {
  GError* error = NULL;
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              power_manager::kPowerManagerServiceName,
                              "/",
                              power_manager::kPowerManagerInterface);
  error = NULL;
  if (!dbus_g_proxy_call(proxy.gproxy(), "UnregisterSuspendDelay",
                         &error,
                         G_TYPE_INVALID, G_TYPE_INVALID)) {
    LOG(ERROR) << "Error Registering: " << error->message;
  }
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

bool SuspendDelaySignaled(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error, DBUS_TYPE_UINT32, &sequence_num,
                             DBUS_TYPE_INVALID)) {
    cout << "Could not get args from SuspendDelay signal!" << endl;
    if (dbus_error_is_set(&error))
      dbus_error_free(&error);
    return true;
  }
  cout << "sequence num = " << sequence_num << endl;
  cout << "sleeping..." << endl;
  g_timeout_add(5000, SendSuspendReady, NULL);
  return true;
}

void OnNameOwnerChanged(DBusGProxy*, const gchar* name, const gchar* old_owner,
                        const gchar* new_owner, gpointer) {
  cout << "name : " << name << endl;
  cout << "old_owner : " << old_owner << endl;
  cout << "new owner : " << new_owner << endl;
  if (0 == strlen(new_owner))
    cout << "BALEETED!" << endl;
}

void RegisterDBusMessageHandler() {
  dbus_handler.SetNameOwnerChangedHandler(OnNameOwnerChanged, NULL);
  dbus_handler.AddDBusSignalHandler(
      power_manager::kPowerManagerInterface,
      power_manager::kSuspendDelay,
      base::Bind(&SuspendDelaySignaled));
  dbus_handler.Start();

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
