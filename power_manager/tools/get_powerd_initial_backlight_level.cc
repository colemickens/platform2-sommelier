// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This program prints the default backlight level that would be used by powerd,
// taking the current prefs and power source and the actual backlight range into
// account.

#include <inttypes.h>

#include <cstdio>
#include <vector>

#include "base/file_path.h"
#include "base/logging.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/policy/internal_backlight_controller.h"
#include "power_manager/powerd/system/ambient_light_sensor_stub.h"
#include "power_manager/powerd/system/backlight_stub.h"
#include "power_manager/powerd/system/display_power_setter_stub.h"
#include "power_manager/powerd/system/internal_backlight.h"
#include "power_manager/powerd/system/power_supply.h"

int main(int argc, char* argv[]) {
  // Get the max and current brightness from the real backlight and use them to
  // initialize a stub backlight (so that InternalBacklightController can do its
  // thing without changing the actual brightness level).
  power_manager::system::InternalBacklight real_backlight;
  CHECK(real_backlight.Init(
     base::FilePath(power_manager::kInternalBacklightPath),
     power_manager::kInternalBacklightPattern));
  int64 max_level = 0, current_level = 0;
  CHECK(real_backlight.GetMaxBrightnessLevel(&max_level));
  CHECK(real_backlight.GetCurrentBrightnessLevel(&current_level));
  power_manager::system::BacklightStub stub_backlight(max_level, current_level);

  power_manager::Prefs prefs;
  CHECK(prefs.Init(power_manager::util::GetPrefPaths(
      power_manager::kReadWritePrefsDir, power_manager::kReadOnlyPrefsDir)));

  scoped_ptr<power_manager::system::AmbientLightSensorStub> light_sensor;
#ifdef HAS_ALS
  light_sensor.reset(new power_manager::system::AmbientLightSensorStub(0));
#endif
  power_manager::system::DisplayPowerSetterStub display_power_setter;
  power_manager::policy::InternalBacklightController backlight_controller(
      &stub_backlight, &prefs, light_sensor.get(), &display_power_setter);
  CHECK(backlight_controller.Init());

  // Get the power source.
  power_manager::system::PowerSupply power_supply(
      base::FilePath(power_manager::kPowerStatusPath), &prefs);
  power_supply.Init();
  power_manager::system::PowerInformation power_info;
  CHECK(power_supply.GetPowerInformation(&power_info));
  const power_manager::PowerSource power_source =
      power_info.power_status.line_power_on ?
      power_manager::POWER_AC : power_manager::POWER_BATTERY;

  // Mimic powerd startup and grab the brightness level that's used.
#ifdef HAS_ALS
  light_sensor->NotifyObservers();
#endif
  backlight_controller.HandlePowerSourceChange(power_source);
  printf("%" PRId64 "\n", stub_backlight.current_level());
  return 0;
}
