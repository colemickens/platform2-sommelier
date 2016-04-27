// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>

#include <base/at_exit.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <brillo/flag_helper.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"

int main(int argc, char* argv[]) {
  DEFINE_bool(ambient_light_sensor, false, "Exit with success if ambient "
              "light sensor support is enabled");
  DEFINE_bool(hover_detection, false, "Exit with success if hover detection "
              "is enabled");
  DEFINE_bool(keyboard_backlight, false, "Exit with success if keyboard "
              "backlight support is enabled");

  brillo::FlagHelper::Init(argc, argv,
      "Check the device's power-related configuration");

  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  CHECK_EQ((FLAGS_ambient_light_sensor ? 1 : 0) +
           (FLAGS_hover_detection ? 1 : 0) +
           (FLAGS_keyboard_backlight ? 1 : 0), 1)
      << "Exactly one flag must be set";

  power_manager::Prefs prefs;
  CHECK(prefs.Init(power_manager::util::GetPrefPaths(
      base::FilePath(power_manager::kReadWritePrefsDir),
      base::FilePath(power_manager::kReadOnlyPrefsDir))));

  if (FLAGS_ambient_light_sensor) {
    bool als_enabled = false;
    prefs.GetBool(power_manager::kHasAmbientLightSensorPref, &als_enabled);
    exit(als_enabled ? 0 : 1);
  } else if (FLAGS_hover_detection) {
    bool hover_enabled = false;
    prefs.GetBool(power_manager::kDetectHoverPref, &hover_enabled);
    exit(hover_enabled ? 0 : 1);
  } else if (FLAGS_keyboard_backlight) {
    bool backlight_enabled = false;
    prefs.GetBool(power_manager::kHasKeyboardBacklightPref, &backlight_enabled);
    exit(backlight_enabled ? 0 : 1);
  } else {
    NOTREACHED();
    exit(1);
  }
}
