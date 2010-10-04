// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "base/logging.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

namespace power_manager {

namespace util {

// interface names
const char* kPowerManagerInterface = "org.chromium.PowerManager";
const char* kLowerPowerManagerInterface = "org.chromium.LowerPowerManager";

// powerd -> powerm signals
const char* kRequestCleanShutdown = "RequestCleanShutdown";
const char* kSuspendSignal = "SuspendSignal";
const char* kShutdownSignal = "ShutdownSignal";

// powerm -> powerd signals
const char* kLidClosed = "LidClosed";
const char* kLidOpened = "LidOpened";
const char* kPowerButtonDown = "PowerButtonDown";
const char* kPowerButtonUp = "PowerButtonUp";

// broadcast signals
const char* kPowerStateChanged = "PowerStateChanged";

bool LoggedIn() {
  return access("/var/run/state/logged-in", F_OK) == 0;
}

void Launch(const char* cmd) {
  LOG(INFO) << "Launching " << cmd;
  pid_t pid = fork();
  if (pid == 0) {
    // Detach from parent so that powerd doesn't need to wait around for us
    setsid();
    exit(fork() == 0 ? system(cmd) : 0);
  } else if (pid > 0) {
    waitpid(pid, NULL, 0);
  }
}

// A utility function to send a signal to the session manager.
void SendSignalToSessionManager(const char* signal) {
  DBusGConnection* connection;
  GError* error = NULL;
  connection = chromeos::dbus::GetSystemBusConnection().g_connection();
  CHECK(connection);
  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      connection,
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath,
      login_manager::kSessionManagerInterface);
  CHECK(proxy);
  error = NULL;
  if (!dbus_g_proxy_call(proxy, signal, &error, G_TYPE_INVALID,
                         G_TYPE_INVALID)) {
    LOG(ERROR) << "Error sending signal: " << error->message;
  }
  g_object_unref(proxy);
}

// A utility function to send a signal to the lower power daemon (powerm).
void SendSignalToPowerM(const char* signal_name) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              "/",
                              util::kLowerPowerManagerInterface);
  DBusMessage* signal = ::dbus_message_new_signal(
      "/",
      util::kLowerPowerManagerInterface,
      signal_name);
  CHECK(signal);
  ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  ::dbus_message_unref(signal);
}

// A utility function to send a signal to upper power daemon (powerd).
void SendSignalToPowerD(const char* signal_name) {
  LOG(INFO) << "Sending signal '" << signal_name << "' to PowerManager:";
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              "/",
                              power_manager::kPowerManagerInterface);
  DBusMessage* signal = ::dbus_message_new_signal(
      "/",
      power_manager::kPowerManagerInterface,
      signal_name);
  CHECK(signal);
  ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  ::dbus_message_unref(signal);
}

}  // namespace util

}  // namespace power_manager
