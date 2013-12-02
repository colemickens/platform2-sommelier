// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus/dbus-glib-lowlevel.h>
#include <gflags/gflags.h>
#include <inttypes.h>
#include <iostream>
#include <string>

#include "base/logging.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/util_dbus.h"

DEFINE_bool(set, false, "Set the percent instead of reading");
DEFINE_double(percent, 0, "Percent to set to");
DEFINE_bool(gradual, true, "Transition gradually");

// Turn the request into a protobuff and send over dbus.
bool GetCurrentBrightness(double* percent) {
  DBusMessage* message = dbus_message_new_method_call(
      power_manager::kPowerManagerServiceName,
      power_manager::kPowerManagerServicePath,
      power_manager::kPowerManagerInterface,
      power_manager::kGetScreenBrightnessPercent);
  DBusMessage* response = power_manager::util::CallDBusMethodAndUnref(message);
  if (!response) {
    LOG(WARNING) << power_manager::kGetScreenBrightnessPercent << " failed";
    return false;
  }
  DBusError error;
  dbus_error_init(&error);
  if (!dbus_message_get_args(response, &error,
                             DBUS_TYPE_DOUBLE, percent,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << "Couldn't read args for response " << error.name
                 << " (" << error.message << ")";
    dbus_error_free(&error);
    dbus_message_unref(response);
    return false;
  }
  dbus_message_unref(response);
  return true;
}

bool SetCurrentBrightness(const double percent, int style) {
  DBusMessage* message = dbus_message_new_method_call(
      power_manager::kPowerManagerServiceName,
      power_manager::kPowerManagerServicePath,
      power_manager::kPowerManagerInterface,
      power_manager::kSetScreenBrightnessPercent);
  dbus_message_append_args(message,
                           DBUS_TYPE_DOUBLE, &percent,
                           DBUS_TYPE_INT32, &style,
                           DBUS_TYPE_INVALID);
  DBusMessage* response = power_manager::util::CallDBusMethodAndUnref(message);
  if (response) {
    dbus_message_unref(response);
    return true;
  }
  return false;
}

// A tool to talk to powerd and get or set the backlight level.
int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK_EQ(argc, 1) << "Unexpected arguments. Try --help";

  g_type_init();

  int style = FLAGS_gradual ? power_manager::kBrightnessTransitionGradual
                            : power_manager::kBrightnessTransitionInstant;
  double percent;
  CHECK(GetCurrentBrightness(&percent)) << "Could not read brightness";
  std::cout << "Current percent = " << percent << std::endl;
  if (FLAGS_set) {
    if (SetCurrentBrightness(FLAGS_percent, style)) {
      std::cout << "Set percent to " << FLAGS_percent << std::endl;
    } else {
      std::cout << "Error setting percent" << std::endl;
    }
    CHECK(GetCurrentBrightness(&percent)) << "Could not read brightness";
    std::cout << "Current percent now = " << percent << std::endl;
  }

  return 0;
}
