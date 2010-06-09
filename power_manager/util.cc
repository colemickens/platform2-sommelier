// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "base/logging.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

namespace power_manager {

namespace util {

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

void SendSignalToSessionManager(const char* signal) {
  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      chromeos::dbus::GetSystemBusConnection().g_connection(),
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath,
      login_manager::kSessionManagerInterface);
  CHECK(proxy);
  GError* error = NULL;
  if (!dbus_g_proxy_call(proxy, signal, &error, G_TYPE_INVALID,
                         G_TYPE_INVALID)) {
    LOG(ERROR) << "Error sending signal: " << error->message;
  }
  g_object_unref(proxy);
}

}  // namespace util

}  // namespace power_manager
