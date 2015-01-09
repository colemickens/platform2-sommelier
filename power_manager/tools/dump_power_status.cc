// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>

#include <base/at_exit.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/flag_helper.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/system/power_supply.h"
#include "power_manager/powerd/system/udev_stub.h"

int main(int argc, char** argv) {
  chromeos::FlagHelper::Init(argc, argv, "Print power information for tests.");
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  power_manager::Prefs prefs;
  CHECK(prefs.Init(power_manager::util::GetPrefPaths(
      base::FilePath(power_manager::kReadWritePrefsDir),
      base::FilePath(power_manager::kReadOnlyPrefsDir))));

  power_manager::system::UdevStub udev;
  base::FilePath path(power_manager::kPowerStatusPath);
  power_manager::system::PowerSupply power_supply;
  power_supply.Init(path, &prefs, &udev, false /* log_shutdown_thresholds */);

  CHECK(power_supply.RefreshImmediately());
  const power_manager::system::PowerStatus status =
      power_supply.GetPowerStatus();

  // Do not change the format of this output without updating tests.
  printf("line_power_connected %d\n", status.line_power_on);
  printf("battery_present %d\n", status.battery_is_present);
  printf("battery_percent %0.2f\n", status.battery_percentage);
  printf("battery_charge %0.2f\n", status.battery_charge);
  printf("battery_charge_full %0.2f\n", status.battery_charge_full);
  printf("battery_charge_full_design %0.2f\n",
         status.battery_charge_full_design);
  printf("battery_energy_rate %0.2f\n", status.battery_energy_rate);
  printf("battery_discharging %d\n",
         status.battery_state ==
         power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);

  return 0;
}
