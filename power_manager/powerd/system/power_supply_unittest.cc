// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/power_supply.h"

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
#include "power_manager/common/test_main_loop_runner.h"
#include "power_manager/powerd/system/udev_stub.h"
#include "power_manager/proto_bindings/power_supply_properties.pb.h"

using std::map;
using std::string;

namespace power_manager {
namespace system {

namespace {

const char kOnline[] = "1";
const char kOffline[] = "0";
const char kPresent[] = "1";
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

class TestObserver : public PowerSupplyObserver {
 public:
  TestObserver() {}
  virtual ~TestObserver() {}

  // Runs the event loop until OnPowerStatusUpdate() is invoked or a timeout is
  // hit. Returns true if the method was invoked and false if it wasn't.
  bool WaitForNotification() {
    return runner_.StartLoop(base::TimeDelta::FromSeconds(10));
  }

  // PowerSupplyObserver overrides:
  virtual void OnPowerStatusUpdate() OVERRIDE {
    runner_.StopLoop();
  }

 private:
  TestMainLoopRunner runner_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

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

    power_supply_.reset(new PowerSupply);
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

  // Initializes |power_supply_|.
  void Init() {
    power_supply_->Init(path_, &prefs_, &udev_);
  }

  // Sets the time so that |power_supply_| will believe that the current
  // has stabilized.
  void SetStabilizedTime() {
    const base::TimeTicks now = test_api_->GetCurrentTime();
    if (power_supply_->battery_stabilized_timestamp() > now)
      test_api_->SetCurrentTime(power_supply_->battery_stabilized_timestamp());
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
  UdevStub udev_;
  scoped_ptr<PowerSupply> power_supply_;
  scoped_ptr<PowerSupply::TestApi> test_api_;
};

// Test system without power supply sysfs (e.g. virtual machine).
TEST_F(PowerSupplyTest, TestNoPowerSupplySysfs) {
  Init();
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

  Init();
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
  Init();
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
  Init();

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
  Init();
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
  Init();

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
  Init();

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
  const base::TimeDelta kStartupDelay = base::TimeDelta::FromSeconds(6);
  const base::TimeDelta kPowerSourceDelay = base::TimeDelta::FromSeconds(7);
  const base::TimeDelta kResumeDelay = base::TimeDelta::FromSeconds(10);
  const base::TimeDelta kSlack = base::TimeDelta::FromMilliseconds(
      PowerSupply::kBatteryStabilizedSlackMs);

  prefs_.SetInt64(kBatteryPollIntervalPref, kPollDelay.InMilliseconds());
  prefs_.SetInt64(kBatteryPollShortIntervalPref,
                  kShortPollDelay.InMilliseconds());
  prefs_.SetInt64(kBatteryStabilizedAfterStartupMsPref,
                  kStartupDelay.InMilliseconds());
  prefs_.SetInt64(kBatteryStabilizedAfterPowerSourceChangeMsPref,
                  kPowerSourceDelay.InMilliseconds());
  prefs_.SetInt64(kBatteryStabilizedAfterResumeMsPref,
                  kResumeDelay.InMilliseconds());

  base::TimeTicks current_time = kStartTime;
  Init();

  // The battery times should be reported as "calculating" just after
  // initialization.
  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.line_power_on);
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ((kStartupDelay + kSlack).InMilliseconds(),
            test_api_->current_poll_delay().InMilliseconds());

  // After enough time has elapsed, the battery times should be reported.
  current_time += kStartupDelay + kSlack;
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
  EXPECT_EQ((kResumeDelay + kSlack).InMilliseconds(),
            test_api_->current_poll_delay().InMilliseconds());

  // Check that the updated times are returned after a delay.
  current_time += kResumeDelay + kSlack;
  test_api_->SetCurrentTime(current_time);
  ASSERT_TRUE(test_api_->TriggerPollTimeout());
  status = power_supply_->power_status();
  EXPECT_FALSE(status.line_power_on);
  EXPECT_FALSE(status.is_calculating_battery_time);

  // Connect AC, report a udev event, and check that the status is updated.
  WriteValue("ac/online", kOnline);
  power_supply_->OnUdevEvent(
      PowerSupply::kUdevSubsystem, "AC", UdevObserver::ACTION_CHANGE);
  status = power_supply_->power_status();
  EXPECT_TRUE(status.line_power_on);
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ((kPowerSourceDelay + kSlack).InMilliseconds(),
            test_api_->current_poll_delay().InMilliseconds());

  // After the delay, estimates should be made again.
  current_time += kPowerSourceDelay + kSlack;
  test_api_->SetCurrentTime(current_time);
  ASSERT_TRUE(test_api_->TriggerPollTimeout());
  status = power_supply_->power_status();
  EXPECT_TRUE(status.line_power_on);
  EXPECT_FALSE(status.is_calculating_battery_time);
}

TEST_F(PowerSupplyTest, UpdateBatteryTimeEstimates) {
  // Start out with the battery 50% full and an unset current.
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", 0.5);
  WriteDoubleValue("battery/current_now", 0.0);
  prefs_.SetDouble(kPowerSupplyFullFactorPref, 1.0);
  Init();

  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // Set the current such that it'll take an hour to charge fully and
  // advance the clock so the current will be used.
  WriteDoubleValue("battery/current_now", 0.5);
  SetStabilizedTime();
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(3600, status.battery_time_to_full.InSeconds());

  // Let half an hour pass and report that the battery is 75% full.
  test_api_->AdvanceTime(base::TimeDelta::FromMinutes(30));
  WriteDoubleValue("battery/charge_now", 0.75);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(1800, status.battery_time_to_full.InSeconds());

  // After a current reading of 1.25, the averaged current should be (0.5 +
  // 0.5 + 1.25) / 3 = 0.75. The remaining 0.25 of charge to get to 100%
  // should take twenty minutes.
  WriteDoubleValue("battery/current_now", 1.25);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(1200, status.battery_time_to_full.InSeconds());

  // Fifteen minutes later, set the current to 0.25 (giving an average of
  // (0.5 + 0.5 + 1.25 + 0.25) / 4 = 0.625) and report an increased charge.
  // There should be 0.125 / 0.625 * 3600 = 720 seconds until the battery
  // is full.
  test_api_->AdvanceTime(base::TimeDelta::FromMinutes(15));
  WriteDoubleValue("battery/current_now", 0.25);
  WriteDoubleValue("battery/charge_now", 0.875);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(720, status.battery_time_to_full.InSeconds());

  // Disconnect the charger and report an immediate drop in charge and
  // current. The current shouldn't be used yet.
  WriteValue("ac/online", kOffline);
  WriteDoubleValue("battery/charge_now", 0.5);
  WriteDoubleValue("battery/current_now", -0.5);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // After the current has had time to stabilize, the average should be
  // reset and the time-to-empty should be estimated.
  SetStabilizedTime();
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(3600, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(3600 - kLowBatteryShutdownTimeSec,
            status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // Thirty minutes later, decrease the charge and report a significantly
  // higher current.
  test_api_->AdvanceTime(base::TimeDelta::FromMinutes(30));
  WriteDoubleValue("battery/charge_now", 0.25);
  WriteDoubleValue("battery/current_now", -1.5);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(900, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(900 - kLowBatteryShutdownTimeSec,
            status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // A current report of 0 should be ignored.
  WriteDoubleValue("battery/current_now", 0.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(900, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(900 - kLowBatteryShutdownTimeSec,
            status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // Suspend, change the current, and resume. The average current should be
  // reset and the battery time should be reported as "calculating".
  power_supply_->SetSuspended(true);
  WriteDoubleValue("battery/current_now", -0.5);
  test_api_->AdvanceTime(base::TimeDelta::FromSeconds(8));
  power_supply_->SetSuspended(false);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // Wait for the current to stabilize.
  SetStabilizedTime();
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(1800, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(1800 - kLowBatteryShutdownTimeSec,
            status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());

  // When a charger is connected but the kernel reports the battery's state
  // as discharging, time-to-empty should be calculated rather than
  // time-to-full.
  WriteValue("ac/online", kOnline);
  WriteValue("battery/status", kDischarging);
  ASSERT_TRUE(UpdateStatus(&status));
  SetStabilizedTime();
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(1800, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(1800 - kLowBatteryShutdownTimeSec,
            status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());
}

TEST_F(PowerSupplyTest, BatteryTimeEstimatesWithZeroCurrent) {
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  WriteDoubleValue("battery/current_now", 0.1 * kEpsilon);
  Init();

  // When the only available current readings are close to 0 (which would
  // result in very large time estimates), -1 estimates should be provided
  // instead.
  SetStabilizedTime();
  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(-1, status.battery_time_to_full.InSeconds());

  WriteValue("ac/online", kOffline);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.is_calculating_battery_time);

  SetStabilizedTime();
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(-1, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(-1, status.battery_time_to_shutdown.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_full.InSeconds());
}

TEST_F(PowerSupplyTest, FullFactor) {
  // When the battery has reached the full factor, it should be reported as
  // fully charged regardless of the current.
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", kFullFactor);
  WriteDoubleValue("battery/current_now", 1.0);
  Init();
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
  Init();

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
  const double kShutdownPercent = 5.0;
  prefs_.SetDouble(kLowBatteryShutdownPercentPref, kShutdownPercent);

  WriteDefaultValues(POWER_BATTERY, REPORT_CHARGE);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", (kShutdownPercent + 1.0) / 100.0);
  WriteDoubleValue("battery/current_now", -1.0);
  Init();

  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.battery_below_shutdown_threshold);

  WriteDoubleValue("battery/charge_now", (kShutdownPercent - 1.0) / 100.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.battery_below_shutdown_threshold);

  // If the charge is zero, assume that something is being misreported and
  // avoid shutting down.
  WriteDoubleValue("battery/charge_now", 0.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.battery_below_shutdown_threshold);

  // Don't shut down when on AC power when the battery's charge isn't observed
  // to be decreasing.
  WriteValue("ac/online", kOnline);
  WriteValue("ac/type", kACType);
  WriteDoubleValue("battery/charge_now", (kShutdownPercent - 1.0) / 100.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.battery_below_shutdown_threshold);

  // Don't shut down for other chargers in this situation, either.
  WriteValue("ac/type", kUSBType);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.battery_below_shutdown_threshold);

  // Test that the system shuts down while on AC power if the charge appears to
  // be falling (i.e. the charger isn't able to deliver enough current).
  SetStabilizedTime();
  WriteValue("ac/type", kACType);
  WriteDoubleValue("battery/charge_now", (kShutdownPercent - 1.0) / 100.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.battery_below_shutdown_threshold);

  // After just half of the observation period has elapsed, the system should
  // still be up.
  const base::TimeDelta kObservationTime = base::TimeDelta::FromMilliseconds(
      PowerSupply::kObservedBatteryChargeRateMinMs);
  WriteDoubleValue("battery/charge_now", (kShutdownPercent - 1.5) / 100.0);
  test_api_->AdvanceTime(kObservationTime / 2);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.battery_below_shutdown_threshold);

  // If the charge is still trending downward after the full observation period
  // has elapsed, the system should shut down.
  WriteDoubleValue("battery/charge_now", (kShutdownPercent - 2.0) / 100.0);
  test_api_->AdvanceTime(kObservationTime / 2);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.battery_below_shutdown_threshold);
}

TEST_F(PowerSupplyTest, LowPowerCharger) {
  // If a charger is connected but the current is zero and the battery
  // isn't full, the battery should be reported as discharging.
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  WriteDoubleValue("battery/current_now", 0.0);
  Init();
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
  Init();

  // Check that the "connected to USB" status is reported for all
  // USB-related strings used by the kernel.
  PowerStatus status;
  const char* kUsbTypes[] = { "USB", "USB_DCP", "USB_CDP", "USB_ACA" };
  for (size_t i = 0; i < arraysize(kUsbTypes); ++i) {
    const char* kType = kUsbTypes[i];
    WriteValue("ac/type", kType);
    ASSERT_TRUE(UpdateStatus(&status)) << "failed for \"" << kType << "\"";
    EXPECT_EQ(PowerSupplyProperties_BatteryState_CHARGING, status.battery_state)
        << "failed for \"" << kType << "\"";
    EXPECT_EQ(PowerSupplyProperties_ExternalPower_USB, status.external_power)
        << "failed for \"" << kType << "\"";
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

TEST_F(PowerSupplyTest, OriginalSpringCharger) {
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  Init();

  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ("", status.line_power_model_name);
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_AC, status.external_power);

  WriteValue("ac/model_name", "0x00");
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ("0x00", status.line_power_model_name);
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_ORIGINAL_SPRING_CHARGER,
            status.external_power);

  WriteValue("ac/model_name", "0x17");
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ("0x17", status.line_power_model_name);
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_ORIGINAL_SPRING_CHARGER,
            status.external_power);

  WriteValue("ac/model_name", "0x1b");
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ("0x1b", status.line_power_model_name);
  EXPECT_EQ(PowerSupplyProperties_ExternalPower_AC, status.external_power);
}

TEST_F(PowerSupplyTest, ShutdownPercentAffectsBatteryTime) {
  static const double kShutdownPercent = 10.0;
  prefs_.SetDouble(kLowBatteryShutdownPercentPref, kShutdownPercent);
  static const double kShutdownSec = 3200;
  prefs_.SetDouble(kLowBatteryShutdownTimePref, kShutdownSec);

  WriteDefaultValues(POWER_BATTERY, REPORT_CHARGE);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", 0.5);
  WriteDoubleValue("battery/current_now", -1.0);
  prefs_.SetDouble(kPowerSupplyFullFactorPref, 1.0);
  Init();
  SetStabilizedTime();

  // The reported time until shutdown should be based only on the charge that's
  // available before shutdown. Note also that the time-based shutdown threshold
  // is ignored since a percent-based threshold is set.
  static const double kShutdownCharge = kShutdownPercent / 100.0 * 1.0;
  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ(1800, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(roundl((0.5 - kShutdownCharge) * 3600),
            status.battery_time_to_shutdown.InSeconds());
  EXPECT_FALSE(status.battery_below_shutdown_threshold);

  // The reported time should be zero once the threshold is reached.
  WriteDoubleValue("battery/charge_now", kShutdownCharge);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ(roundl(kShutdownCharge / 1.0 * 3600),
            status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_TRUE(status.battery_below_shutdown_threshold);

  // It should remain zero if the threshold is passed.
  static const double kLowerCharge = kShutdownCharge / 2.0;
  WriteDoubleValue("battery/charge_now", kLowerCharge);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ(roundl(kLowerCharge / 1.0 * 3600),
            status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_TRUE(status.battery_below_shutdown_threshold);
}

TEST_F(PowerSupplyTest, ObservedBatteryChargeRate) {
  // Make sure that the sampling window is long enough to hold all of the
  // samples generated by this test.
  ASSERT_GE(PowerSupply::kMaxChargeSamples, 4);

  WriteDefaultValues(POWER_BATTERY, REPORT_CHARGE);
  WriteDoubleValue("battery/charge_full", 10.0);
  WriteDoubleValue("battery/charge_now", 10.0);
  prefs_.SetDouble(kPowerSupplyFullFactorPref, 1.0);
  Init();
  SetStabilizedTime();

  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(0.0, status.observed_battery_charge_rate);

  // Advance the time, but not by enough to estimate the rate.
  const base::TimeDelta kObservationTime = base::TimeDelta::FromMilliseconds(
      PowerSupply::kObservedBatteryChargeRateMinMs);
  test_api_->AdvanceTime(kObservationTime / 2);
  WriteDoubleValue("battery/charge_now", 9.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(0.0, status.observed_battery_charge_rate);

  // Advance the time by enough so the next reading will be a full hour from the
  // first one, indicating that the charge is dropping by 1 Ah per hour.
  test_api_->AdvanceTime(base::TimeDelta::FromHours(1) - kObservationTime / 2);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(-1.0, status.observed_battery_charge_rate);

  // Decrease the charge by 3 Ah over the next hour.
  test_api_->AdvanceTime(base::TimeDelta::FromHours(1));
  WriteDoubleValue("battery/charge_now", 6.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(-2.0, status.observed_battery_charge_rate);

  // Switch to AC power and report a different charge. The rate should be
  // reported as 0 initially.
  WriteValue("ac/online", kOnline);
  WriteDoubleValue("battery/charge_now", 7.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(0.0, status.observed_battery_charge_rate);

  // Let enough time pass for the battery readings to stabilize.
  SetStabilizedTime();
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(0.0, status.observed_battery_charge_rate);

  // Advance the time just enough for the rate to be calculated and increase the
  // charge by 1 Ah.
  test_api_->AdvanceTime(kObservationTime);
  WriteDoubleValue("battery/charge_now", 8.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(1.0 / (kObservationTime.InSecondsF() / 3600),
                   status.observed_battery_charge_rate);

  // Now advance the time to get a reading one hour from the first one and
  // decrease the charge by 2 Ah from the first reading while on AC power.
  test_api_->AdvanceTime(base::TimeDelta::FromHours(1) - kObservationTime);
  WriteDoubleValue("battery/charge_now", 5.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(-2.0, status.observed_battery_charge_rate);

  // Send enough identical samples to fill the window and check that the rate is
  // reported as 0.
  for (int i = 0; i < PowerSupply::kMaxChargeSamples; ++i) {
    test_api_->AdvanceTime(base::TimeDelta::FromHours(1));
    ASSERT_TRUE(UpdateStatus(&status));
  }
  EXPECT_DOUBLE_EQ(0.0, status.observed_battery_charge_rate);
}

TEST_F(PowerSupplyTest, LowBatteryShutdownSafetyPercent) {
  // Start out discharging on AC with a ludicrously-high current where all of
  // the charge will be drained in a minute.
  WriteDefaultValues(POWER_AC, REPORT_CHARGE);
  WriteValue("battery/status", kDischarging);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", 0.5);
  WriteDoubleValue("battery/current_now", -60.0);
  prefs_.SetInt64(kLowBatteryShutdownTimePref, 180);
  prefs_.SetDouble(kPowerSupplyFullFactorPref, 1.0);
  Init();

  // The system shouldn't shut down initially since it's on AC power and a
  // negative charge rate hasn't yet been observed.
  SetStabilizedTime();
  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ(30, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_DOUBLE_EQ(0.0, status.observed_battery_charge_rate);
  EXPECT_FALSE(status.battery_below_shutdown_threshold);

  // Even after a negative charge rate is observed, the system still shouldn't
  // shut down, since the battery percent is greater than the safety percent.
  test_api_->AdvanceTime(base::TimeDelta::FromMilliseconds(
      PowerSupply::kObservedBatteryChargeRateMinMs));
  WriteDoubleValue("battery/charge_now", 0.25);
  ASSERT_GT(25.0, PowerSupply::kLowBatteryShutdownSafetyPercent);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_EQ(15, status.battery_time_to_empty.InSeconds());
  EXPECT_EQ(0, status.battery_time_to_shutdown.InSeconds());
  EXPECT_LT(status.observed_battery_charge_rate, 0.0);
  EXPECT_FALSE(status.battery_below_shutdown_threshold);
}

TEST_F(PowerSupplyTest, NotifyObserver) {
  // Set a long polling delay to ensure that PowerSupply doesn't poll in the
  // background during the test.
  const base::TimeDelta kDelay = base::TimeDelta::FromSeconds(60);
  prefs_.SetInt64(kBatteryPollIntervalPref, kDelay.InMilliseconds());
  prefs_.SetInt64(kBatteryPollShortIntervalPref, kDelay.InMilliseconds());
  prefs_.SetInt64(kBatteryStabilizedAfterStartupMsPref,
                  kDelay.InMilliseconds());

  // Check that observers are notified about updates asynchronously.
  TestObserver observer;
  power_supply_->AddObserver(&observer);
  Init();
  ASSERT_TRUE(power_supply_->RefreshImmediately());
  EXPECT_TRUE(observer.WaitForNotification());
  power_supply_->RemoveObserver(&observer);
}

TEST_F(PowerSupplyTest, RegisterForUdevEvents) {
  Init();
  EXPECT_TRUE(udev_.HasObserver(PowerSupply::kUdevSubsystem,
                                power_supply_.get()));

  PowerSupply* dead_ptr = power_supply_.get();
  power_supply_.reset();
  EXPECT_FALSE(udev_.HasObserver(PowerSupply::kUdevSubsystem, dead_ptr));
}

}  // namespace system
}  // namespace power_manager
