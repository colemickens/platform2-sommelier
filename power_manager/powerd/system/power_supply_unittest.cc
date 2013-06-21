// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/power_supply.h"

#include <glib.h>
#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <string>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "power_manager/common/clock.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/power_supply_properties.pb.h"

using std::map;
using std::string;

namespace power_manager {
namespace system {

namespace {

const char kOnline[] = "1";
const char kOffline[] = "0";
const char kPresent[] = "1";
const char kNotPresent[] = "0";
const char kACType[] = "Mains";
const char kBatteryType[] = "Battery";
const char kUSBType[] = "USB";
const char kCharging[] = "Charging";
const char kDischarging[] = "Discharging";

const double kChargeFull = 2.40;
const double kChargeNow = 1.80;
const double kCurrentNow = 0.20;
const double kVoltageNow = 2.50;

const double kPowerNow = kCurrentNow * kVoltageNow;
const double kEnergyNow = kChargeNow * kVoltageNow;
const double kEnergyFull = kChargeFull * kVoltageNow;
const double kEnergyRate = kCurrentNow * kVoltageNow;
const double kPercentage = 100. * kChargeNow / kChargeFull;

// sysfs stores doubles by multiplying them by 1000000 and storing as an int.
int ScaleDouble(double value) {
  return round(value * 1000000);
}

const int kPowerNowInt = ScaleDouble(kPowerNow);
const int kEnergyFullInt = ScaleDouble(kEnergyFull);
const int kEnergyNowInt = ScaleDouble(kEnergyNow);
const int kChargeFullInt = ScaleDouble(kChargeFull);
const int kChargeNowInt = ScaleDouble(kChargeNow);
const int kCurrentNowInt = ScaleDouble(kCurrentNow);
const int kVoltageNowInt = ScaleDouble(kVoltageNow);

const int kCycleCount = 10000;

// Default value for kLowBatteryShutdownTimePref.
const int64 kLowBatteryShutdownTimeSec = 180;

// Default value for kPowerSupplyFullFactorPref.
const double kFullFactor = 0.98;

// Battery time-to-full and time-to-empty estimates, in seconds, when using
// the above constants.
const int64 kTimeToFull =
    lround(3600. * (kChargeFull * kFullFactor - kChargeNow) / kCurrentNow);
const int64 kTimeToEmpty = lround(3600. * (kChargeNow) / kCurrentNow);

// Starting value used by |power_supply_| as "now".
const base::TimeTicks kStartTime = base::TimeTicks::FromInternalValue(1000);

}  // namespace

class PowerSupplyTest : public ::testing::Test {
 public:
  PowerSupplyTest() {}

  virtual void SetUp() {
    temp_dir_generator_.reset(new base::ScopedTempDir());
    ASSERT_TRUE(temp_dir_generator_->CreateUniqueTempDir());
    EXPECT_TRUE(temp_dir_generator_->IsValid());
    path_ = temp_dir_generator_->path();

    prefs_.SetInt64(kLowBatteryShutdownTimePref, kLowBatteryShutdownTimeSec);
    prefs_.SetDouble(kPowerSupplyFullFactorPref, kFullFactor);

    power_supply_.reset(new PowerSupply(path_, &prefs_));
    test_api_.reset(new PowerSupply::TestApi(power_supply_.get()));
    test_api_->SetCurrentTime(kStartTime);
  }

 protected:
  // Passed to WriteDefaultValues() to specify how the battery level should
  // be reported.
  enum ReportType {
    REPORT_CHARGE,
    REPORT_ENERGY
  };

  // Sets the time so that |power_supply_| will believe that the current
  // has stabilized after startup.
  void SetStabilizedTime() {
    test_api_->SetCurrentTime(
        kStartTime + power_supply_->current_stabilized_delay());
  }

  void WriteValue(const std::string& relative_filename,
                  const std::string& value) {
    CHECK(file_util::WriteFile(path_.Append(relative_filename),
                               value.c_str(), value.length()));
  }

  void WriteDoubleValue(const std::string& relative_filename, double value) {
    WriteValue(relative_filename, base::IntToString(ScaleDouble(value)));
  }

  // Writes the entries in |value_map| to |path_|.  Keys are relative
  // filenames and values are the values to be written.
  void WriteValues(const map<string, string>& values) {
    for (map<string, string>::const_iterator iter = values.begin();
         iter != values.end(); ++iter) {
      WriteValue(iter->first, iter->second);
    }
  }

  void WriteDefaultValues(PowerSource source, ReportType report_type) {
    file_util::CreateDirectory(path_.Append("ac"));
    file_util::CreateDirectory(path_.Append("battery"));
    map<string, string> values;
    values["ac/online"] = source == POWER_AC ? kOnline : kOffline;
    values["ac/type"] = kACType;
    values["battery/type"] = kBatteryType;
    values["battery/present"] = kPresent;
    values["battery/status"] = source == POWER_AC ? kCharging : kDischarging;
    if (report_type == REPORT_CHARGE) {
      values["battery/charge_full"] = base::IntToString(kChargeFullInt);
      values["battery/charge_full_design"] = base::IntToString(kChargeFullInt);
      values["battery/charge_now"] = base::IntToString(kChargeNowInt);
      values["battery/current_now"] = base::IntToString(kCurrentNowInt);
    } else {
      values["battery/energy_full"] = base::IntToString(kEnergyFullInt);
      values["battery/energy_full_design"] = base::IntToString(kEnergyFullInt);
      values["battery/energy_now"] = base::IntToString(kEnergyNowInt);
      values["battery/power_now"] = base::IntToString(kPowerNowInt);
    }
    values["battery/voltage_now"] = base::IntToString(kVoltageNowInt);
    values["battery/voltage_min_design"] = base::IntToString(kVoltageNowInt);
    values["battery/cycle_count"] = base::IntToString(kCycleCount);
    WriteValues(values);
  }

  // Refreshes and updates |status|.  Returns false if the refresh failed.
  bool UpdateStatus(PowerStatus* status) WARN_UNUSED_RESULT {
    CHECK(status);
    if (!power_supply_->RefreshImmediately())
      return false;

    *status = power_supply_->power_status();
    return true;
  }

  FakePrefs prefs_;
  scoped_ptr<base::ScopedTempDir> temp_dir_generator_;
  FilePath path_;
  scoped_ptr<PowerSupply> power_supply_;
  scoped_ptr<PowerSupply::TestApi> test_api_;
};

// Test system without power supply sysfs (e.g. virtual machine).
TEST_F(PowerSupplyTest, TestNoPowerSupplySysfs) {
  power_supply_->Init();
  PowerStatus power_status;
  ASSERT_TRUE(UpdateStatus(&power_status));
  // In absence of power supply sysfs, default assumption is line power on, no
  // battery present.
  EXPECT_TRUE(power_status.line_power_on);
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_AC,
            power_status.external_power);
  EXPECT_FALSE(power_status.battery_is_present);
  EXPECT_EQ(PowerSupplyProperties_BatteryState_NOT_PRESENT,
            power_status.battery_state);
}

// Test line power without battery.
TEST_F(PowerSupplyTest, TestNoBattery) {
  file_util::CreateDirectory(path_.Append("ac"));
  map<string, string> values;
  values["ac/online"] = kOnline;
  values["ac/type"] = kACType;
  WriteValues(values);

  power_supply_->Init();
  PowerStatus power_status;
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_TRUE(power_status.line_power_on);
  EXPECT_EQ(kACType, power_status.line_power_type);
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_AC,
            power_status.external_power);
  EXPECT_FALSE(power_status.battery_is_present);
  EXPECT_EQ(PowerSupplyProperties_BatteryState_NOT_PRESENT,
            power_status.battery_state);
}

// When neither a line power source nor a battery is found, PowerSupply
// should assume that AC power is being used.
TEST_F(PowerSupplyTest, TestNoPowerSource) {
  power_supply_->Init();
  PowerStatus power_status;
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_TRUE(power_status.line_power_on);
  EXPECT_EQ(kACType, power_status.line_power_type);
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_AC,
            power_status.external_power);
  EXPECT_FALSE(power_status.battery_is_present);
  EXPECT_EQ(PowerSupplyProperties_BatteryState_NOT_PRESENT,
            power_status.battery_state);
}

// Test battery charging status.
TEST_F(PowerSupplyTest, TestCharging) {
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  power_supply_->Init();

  SetStabilizedTime();
  PowerStatus power_status;
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_TRUE(power_status.line_power_on);
  EXPECT_EQ(kACType, power_status.line_power_type);
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_AC,
            power_status.external_power);
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_EQ(PowerSupplyProperties_BatteryState_CHARGING,
            power_status.battery_state);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToFull, power_status.battery_time_to_full.InSeconds());
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
}

// Tests that the line power source doesn't need to be named "Mains".
TEST_F(PowerSupplyTest, TestNonMainsLinePower) {
  const char kACArbType[] = "ArbitraryName";
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  WriteValue("ac/type", kACArbType);
  power_supply_->Init();
  PowerStatus power_status;
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_TRUE(power_status.line_power_on);
  EXPECT_EQ(kACArbType, power_status.line_power_type);
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_AC,
            power_status.external_power);
  EXPECT_TRUE(power_status.battery_is_present);
}

// Test battery discharging status.  Test both positive and negative current
// values.
TEST_F(PowerSupplyTest, TestDischarging) {
  WriteDefaultValues(POWER_BATTERY, REPORT_CHARGE);
  power_supply_->Init();

  SetStabilizedTime();
  PowerStatus power_status;
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_EQ(PowerSupplyProperties_BatteryState_DISCHARGING,
            power_status.battery_state);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty,
                   power_status.battery_time_to_empty.InSeconds());
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);

  WriteValue("battery/current_now", base::IntToString(-kCurrentNowInt));
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_FALSE(power_status.line_power_on);
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_DISCONNECTED,
            power_status.external_power);
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_EQ(PowerSupplyProperties_BatteryState_DISCHARGING,
            power_status.battery_state);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty,
                   power_status.battery_time_to_empty.InSeconds());
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
}

// Test battery reporting energy instead of charge.
TEST_F(PowerSupplyTest, TestEnergyDischarging) {
  WriteDefaultValues(POWER_BATTERY, REPORT_ENERGY);
  power_supply_->Init();

  SetStabilizedTime();
  PowerStatus power_status;
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_EQ(PowerSupplyProperties_BatteryState_DISCHARGING,
            power_status.battery_state);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty,
                   power_status.battery_time_to_empty.InSeconds());
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);

  WriteValue("battery/power_now", base::IntToString(-kPowerNowInt));
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_FALSE(power_status.line_power_on);
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_EQ(PowerSupplyProperties_BatteryState_DISCHARGING,
            power_status.battery_state);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty,
                   power_status.battery_time_to_empty.InSeconds());
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
}

TEST_F(PowerSupplyTest, PollDelays) {
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);

  const base::TimeDelta kPollDelay = base::TimeDelta::FromSeconds(30);
  const base::TimeDelta kShortPollDelay = base::TimeDelta::FromSeconds(5);
  const base::TimeDelta kShortPollDelayPlusSlack = kShortPollDelay +
      base::TimeDelta::FromMilliseconds(PowerSupply::kCurrentStabilizedSlackMs);

  prefs_.SetInt64(kBatteryPollIntervalPref, kPollDelay.InMilliseconds());
  prefs_.SetInt64(kBatteryPollShortIntervalPref,
                   kShortPollDelay.InMilliseconds());

  base::TimeTicks current_time = kStartTime;
  power_supply_->Init();

  // Make sure that the initial polling delay will advance the time enough
  // for the battery time to be estimated.
  ASSERT_GE(kShortPollDelayPlusSlack.InMilliseconds(),
            power_supply_->current_stabilized_delay().InMilliseconds());

  // The battery times should be reported as "calculating" just after
  // initialization.
  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.line_power_on);
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(kShortPollDelayPlusSlack.InMilliseconds(),
            test_api_->current_poll_delay().InMilliseconds());

  // After enough time has elapsed, the battery times should be reported.
  current_time += kShortPollDelayPlusSlack;
  test_api_->SetCurrentTime(current_time);
  ASSERT_TRUE(test_api_->TriggerPollTimeout());
  status = power_supply_->power_status();
  EXPECT_TRUE(status.line_power_on);
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(kPollDelay.InMilliseconds(),
            test_api_->current_poll_delay().InMilliseconds());

  // Polling should stop when the system is about to suspend.
  power_supply_->SetSuspended(true);
  EXPECT_EQ(0, test_api_->current_poll_delay().InMilliseconds());

  // After resuming, the status should be updated immediately and the
  // battery times should be reported as "calculating" again.
  current_time += base::TimeDelta::FromSeconds(120);
  test_api_->SetCurrentTime(current_time);
  WriteValue("ac/online", kOffline);
  power_supply_->SetSuspended(false);
  status = power_supply_->power_status();
  EXPECT_FALSE(status.line_power_on);
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(kShortPollDelayPlusSlack.InMilliseconds(),
            test_api_->current_poll_delay().InMilliseconds());

  // Check that the updated times are returned after a delay.
  current_time += kShortPollDelayPlusSlack;
  test_api_->SetCurrentTime(current_time);
  ASSERT_TRUE(test_api_->TriggerPollTimeout());
  status = power_supply_->power_status();
  EXPECT_FALSE(status.line_power_on);
  EXPECT_FALSE(status.is_calculating_battery_time);

  // Connect AC, report a udev event, and check that the status is updated.
  WriteValue("ac/online", kOnline);
  power_supply_->HandleUdevEvent();
  status = power_supply_->power_status();
  EXPECT_TRUE(status.line_power_on);
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(kShortPollDelayPlusSlack.InMilliseconds(),
            test_api_->current_poll_delay().InMilliseconds());

  // After the delay, estimates should be made again.
  current_time += kShortPollDelayPlusSlack;
  test_api_->SetCurrentTime(current_time);
  ASSERT_TRUE(test_api_->TriggerPollTimeout());
  status = power_supply_->power_status();
  EXPECT_TRUE(status.line_power_on);
  EXPECT_FALSE(status.is_calculating_battery_time);
}

TEST_F(PowerSupplyTest, UpdateBatteryTimeEstimates) {
  const base::TimeDelta kStabilizeDelay =
      power_supply_->current_stabilized_delay();

  // Start out with the battery 50% full and an unset current.
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", 0.5);
  WriteDoubleValue("battery/current_now", 0.0);
  prefs_.SetDouble(kPowerSupplyFullFactorPref, 1.0);
  power_supply_->Init();

  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // Set the current such that it'll take an hour to charge fully and
  // advance the clock so the current will be used.
  WriteDoubleValue("battery/current_now", 0.5);
  base::TimeTicks now = kStartTime + kStabilizeDelay;
  test_api_->SetCurrentTime(now);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(3600, status.battery_time_to_full.InSeconds());

  // Let half an hour pass and report that the battery is 75% full.
  now += base::TimeDelta::FromMinutes(30);
  test_api_->SetCurrentTime(now);
  WriteDoubleValue("battery/charge_now", 0.75);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(1800, status.battery_time_to_full.InSeconds());

  // After a current reading of 1.25, the averaged current should be (0.5 +
  // 0.5 + 1.25) / 3 = 0.75. The remaining 0.25 of charge to get to 100%
  // should take twenty minutes.
  WriteDoubleValue("battery/current_now", 1.25);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(1200, status.battery_time_to_full.InSeconds());

  // Fifteen minutes later, set the current to 0.25 (giving an average of
  // (0.5 + 0.5 + 1.25 + 0.25) / 4 = 0.625) and report an increased charge.
  // There should be 0.125 / 0.625 * 3600 = 720 seconds until the battery
  // is full.
  now += base::TimeDelta::FromMinutes(15);
  test_api_->SetCurrentTime(now);
  WriteDoubleValue("battery/current_now", 0.25);
  WriteDoubleValue("battery/charge_now", 0.875);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(720, status.battery_time_to_full.InSeconds());

  // Disconnect the charger and report an immediate drop in charge and
  // current. The current shouldn't be used yet.
  WriteValue("ac/online", kOffline);
  WriteDoubleValue("battery/charge_now", 0.5);
  WriteDoubleValue("battery/current_now", -0.5);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // After the current has had time to stabilize, the average should be
  // reset and the time-to-empty should be estimated.
  now += kStabilizeDelay;
  test_api_->SetCurrentTime(now);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(3600, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // Thirty minutes later, decrease the charge and report a significantly
  // higher current.
  now += base::TimeDelta::FromMinutes(30);
  test_api_->SetCurrentTime(now);
  WriteDoubleValue("battery/charge_now", 0.25);
  WriteDoubleValue("battery/current_now", -1.5);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(900, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // A current report of 0 should be ignored.
  WriteDoubleValue("battery/current_now", 0.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(900, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // Suspend, change the current, and resume. The average current should be
  // reset and the battery time should be reported as "calculating".
  power_supply_->SetSuspended(true);
  WriteDoubleValue("battery/current_now", -0.5);
  now += base::TimeDelta::FromSeconds(8);
  test_api_->SetCurrentTime(now);
  power_supply_->SetSuspended(false);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // Wait for the current to stabilize.
  now += kStabilizeDelay;
  test_api_->SetCurrentTime(now);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(1800, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // When a charger is connected but the kernel reports the battery's state
  // as discharging, time-to-empty should be calculated rather than
  // time-to-full.
  WriteValue("ac/online", kOnline);
  WriteValue("battery/status", kDischarging);
  ASSERT_TRUE(UpdateStatus(&status));
  now += kStabilizeDelay;
  test_api_->SetCurrentTime(now);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(1800, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());
}

TEST_F(PowerSupplyTest, BatteryTimeEstimatesWithZeroCurrent) {
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  WriteDoubleValue("battery/current_now", 0.1 * kEpsilon);
  power_supply_->Init();

  // When the only available current readings are close to 0 (which would
  // result in very large time estimates), -1 estimates should be provided
  // instead.
  base::TimeTicks now = kStartTime + power_supply_->current_stabilized_delay();
  test_api_->SetCurrentTime(now);
  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(-1, status.battery_time_to_full.InSeconds());

  WriteValue("ac/online", kOffline);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.is_calculating_battery_time);

  now += power_supply_->current_stabilized_delay();
  test_api_->SetCurrentTime(now);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(-1, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());
}

TEST_F(PowerSupplyTest, FullFactor) {
  // When the battery has reached the full factor, it should be reported as
  // fully charged regardless of the current.
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", kFullFactor);
  WriteDoubleValue("battery/current_now", 1.0);
  power_supply_->Init();
  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ(PowerSupplyProperties_BatteryState_FULL, status.battery_state);
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);

  // It should stay full when the current goes to zero.
  WriteDoubleValue("battery/current_now", 0.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ(PowerSupplyProperties_BatteryState_FULL, status.battery_state);
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);
}

TEST_F(PowerSupplyTest, DisplayBatteryPercent) {
  static const double kShutdownPercent = 5.0;
  prefs_.SetDouble(kLowBatteryShutdownPercentPref, kShutdownPercent);

  // Start out with a full battery on AC power.
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", 1.0);
  WriteDoubleValue("battery/current_now", 0.0);
  power_supply_->Init();

  // 100% should be reported both on AC and battery power.
  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);
  WriteValue("ac/online", kOffline);
  WriteDoubleValue("battery/current_now", -1.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);

  // Decrease the battery charge, but keep it above the full-factor-derived
  // "full" threshold. Batteries sometimes report a lower charge as soon
  // as line power has been disconnected.
  const double kFullCharge = kFullFactor;
  WriteDoubleValue("battery/charge_now", kFullCharge);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);

  // Lower charges should be scaled.
  const double kLowerCharge = 0.92;
  WriteDoubleValue("battery/charge_now", kLowerCharge);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0 * (100.0 * kLowerCharge - kShutdownPercent) /
                   (100.0 * kFullFactor - kShutdownPercent),
                   status.display_battery_percentage);

  // Switch to AC and check that the scaling remains the same.
  WriteValue("ac/online", kOnline);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0 * (100.0 * kLowerCharge - kShutdownPercent) /
                   (100.0 * kFullFactor - kShutdownPercent),
                   status.display_battery_percentage);

  WriteDoubleValue("battery/charge_now", 0.85);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100 * (85.0 - kShutdownPercent) /
                   (100.0 * kFullFactor - kShutdownPercent),
                   status.display_battery_percentage);

  WriteDoubleValue("battery/charge_now", kShutdownPercent / 100.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(0.0, status.display_battery_percentage);

  WriteDoubleValue("battery/charge_now", 0.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(0.0, status.display_battery_percentage);

  WriteDoubleValue("battery/charge_now", -0.1);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(0.0, status.display_battery_percentage);
}

TEST_F(PowerSupplyTest, CheckForLowBattery) {
  base::TimeTicks kStartTime = base::TimeTicks::FromInternalValue(1000);
  test_api_->SetCurrentTime(kStartTime);

  const double kShutdownPercent = 5.0;
  prefs_.SetDouble(kLowBatteryShutdownPercentPref, kShutdownPercent);

  WriteDefaultValues(POWER_BATTERY, REPORT_CHARGE);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", (kShutdownPercent + 1.0) / 100.0);
  WriteDoubleValue("battery/current_now", -1.0);
  power_supply_->Init();

  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.battery_below_shutdown_threshold);

  WriteDoubleValue("battery/charge_now", (kShutdownPercent - 1.0) / 100.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.battery_below_shutdown_threshold);

  // Don't shut down when on AC power.
  WriteValue("ac/online", kOnline);
  WriteValue("ac/type", kACType);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.battery_below_shutdown_threshold);

  // Shut down when on other chargers, though.
  WriteValue("ac/type", kUSBType);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.battery_below_shutdown_threshold);

  // If the charge is zero, assume that something is being misreported and
  // avoid shutting down.
  WriteDoubleValue("battery/charge_now", 0.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.battery_below_shutdown_threshold);
}

TEST_F(PowerSupplyTest, LowPowerCharger) {
  // If a charger is connected but the current is zero and the battery
  // isn't full, the battery should be reported as discharging.
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  WriteDoubleValue("battery/current_now", 0.0);
  power_supply_->Init();
  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_AC,
            status.external_power);
  EXPECT_EQ(PowerSupplyProperties_BatteryState_DISCHARGING,
            status.battery_state);

  // If the current is nonzero but the kernel-reported status is
  // "Discharging", the battery should be reported as discharging.
  WriteValue("battery/status", kDischarging);
  WriteDoubleValue("battery/current_now", 1.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_AC,
            status.external_power);
  EXPECT_EQ(PowerSupplyProperties_BatteryState_DISCHARGING,
            status.battery_state);
}

TEST_F(PowerSupplyTest, ConnectedToUsb) {
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  power_supply_->Init();

  // Check that the "connected to USB" status is reported for all
  // USB-related strings used by the kernel.
  PowerStatus status;
  const char* kUsbTypes[] = { "USB", "USB_DCP", "USB_CDP", "USB_ACA" };
  for (size_t i = 0; i < arraysize(kUsbTypes); ++i) {
    const char* kType = kUsbTypes[i];
    WriteValue("ac/type", kType);
    ASSERT_TRUE(UpdateStatus(&status)) << "failed for \"" << kType << "\"";
    EXPECT_EQ(PowerSupplyProperties_BatteryState_CHARGING,
              status.battery_state) << "failed for \"" << kType << "\"";
    EXPECT_EQ(PowerSupplyProperties_ExternalPower_USB,
              status.external_power) << "failed for \"" << kType << "\"";
  }

  // The USB type should be reported even when the current is 0.
  WriteValue("ac/type", "USB");
  WriteDoubleValue("battery/current_now", 0.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ(PowerSupplyProperties_BatteryState_DISCHARGING,
            status.battery_state);
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_USB,
            status.external_power);
}

}  // namespace system
}  // namespace power_manager
