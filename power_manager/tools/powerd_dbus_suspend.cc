// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is meant for debugging use to manually trigger a proper
// suspend, excercising the full path through the power manager.
// The actual work to suspend the system is done by powerd_suspend.
// This tool will block and only exit after it has received a DBus
// resume signal from powerd_suspend.

#include <cstdio>
#include <cstdlib>
#include <dbus/dbus.h>
#include <gflags/gflags.h>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chromeos/dbus/service_constants.h"

DEFINE_int32(timeout, 0, "Timeout in seconds.");

int main(int argc, char* argv[]) {
  // Initialization
  google::ParseCommandLineFlags(&argc, &argv, true);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(-1);
  base::TimeTicks end;
  if (FLAGS_timeout) {
    timeout = base::TimeDelta::FromSeconds(FLAGS_timeout);
    end = base::TimeTicks::Now() + timeout;
  }

  DBusError error;
  dbus_error_init(&error);
  DBusConnection* connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
  CHECK(!dbus_error_is_set(&error)) << error.name << ": " << error.message;

  // Listen to resume signal
  std::string match = base::StringPrintf("type='signal',interface='%s',"
      "member='%s',arg0='on'", power_manager::kPowerManagerInterface,
      power_manager::kPowerStateChangedSignal);
  dbus_bus_add_match(connection, match.c_str(), &error);
  CHECK(!dbus_error_is_set(&error)) << error.name << ": " << error.message;

  // Send suspend signal
  DBusMessage* message = dbus_message_new_signal("/",
      power_manager::kPowerManagerInterface,
      power_manager::kRequestSuspendSignal);
  CHECK(message && dbus_connection_send(connection, message, NULL));
  dbus_message_unref(message);

  // Process queued up operations and wait for signal
  do {
    CHECK(dbus_connection_read_write(connection, timeout.InMilliseconds()))
        << "DBusConnection closed before receiving resume signal.";
    while ((message = dbus_connection_pop_message(connection))) {
      if (dbus_message_is_signal(message,
                                 power_manager::kPowerManagerInterface,
                                 power_manager::kPowerStateChangedSignal)) {
        exit(0);
      }
      dbus_message_unref(message);
    }
    if (!FLAGS_timeout)
      continue;
    timeout = end - base::TimeTicks::Now();
  } while (timeout > base::TimeDelta());

  LOG(FATAL) << "Did not receive a resume message within the timeout.";
}
