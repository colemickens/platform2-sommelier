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
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/signal_callback.h"

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

const double kChargeFull = 2.40;
const double kChargeNow = 1.80;
const double kCurrentNow = 0.20;
const double kVoltageNow = 2.50;

const double kPowerNow = kCurrentNow * kVoltageNow;
const double kEnergyNow = kChargeNow * kVoltageNow;
const double kEnergyFull = kChargeFull * kVoltageNow;
const double kEnergyRate = kCurrentNow * kVoltageNow;
const double kPercentage = 100. * kChargeNow / kChargeFull;
const int64 kTimeToFull =
    lround(3600. * (kChargeFull - kChargeNow) / kCurrentNow);
const int64 kTimeToEmpty = lround(3600. * (kChargeNow) / kCurrentNow);

// sysfs stores doubles by multiplying them by 1000000 and storing as an int.
const int kDoubleScaleFactor = 1000000;
int ScaleDouble(double value) {
  return round(value * kDoubleScaleFactor);
}

const int kPowerNowInt = ScaleDouble(kPowerNow);
const int kEnergyFullInt = ScaleDouble(kEnergyFull);
const int kEnergyNowInt = ScaleDouble(kEnergyNow);
const int kChargeFullInt = ScaleDouble(kChargeFull);
const int kChargeNowInt = ScaleDouble(kChargeNow);
const int kCurrentNowInt = ScaleDouble(kCurrentNow);
const int kVoltageNowInt = ScaleDouble(kVoltageNow);

const int kCycleCount = 10000;

const unsigned int kSampleWindowMax = 10;
const unsigned int kSampleWindowMin = 1;

const unsigned int kTaperTimeMax = 60*60;
const unsigned int kTaperTimeMin = 10*60;

// Default value for kLowBatteryShutdownTimePref.
const int64 kLowBatteryShutdownTimeSec = 180;

}  // namespace

class PowerSupplyTest : public ::testing::Test {
 public:
  PowerSupplyTest() {}

  virtual void SetUp() {
    temp_dir_generator_.reset(new base::ScopedTempDir());
    ASSERT_TRUE(temp_dir_generator_->CreateUniqueTempDir());
    EXPECT_TRUE(temp_dir_generator_->IsValid());
    path_ = temp_dir_generator_->path();

    prefs_.SetInt64(kSampleWindowMaxPref, kSampleWindowMax);
    prefs_.SetInt64(kSampleWindowMinPref, kSampleWindowMin);
    prefs_.SetInt64(kTaperTimeMaxPref, kTaperTimeMax);
    prefs_.SetInt64(kTaperTimeMinPref, kTaperTimeMin);
    prefs_.SetInt64(kLowBatteryShutdownTimePref, kLowBatteryShutdownTimeSec);

    power_supply_.reset(new PowerSupply(path_, &prefs_));
    test_api_.reset(new PowerSupply::TestApi(power_supply_.get()));
  }

 protected:
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

  void WriteDefaultValues(bool on_ac, bool report_charge) {
    file_util::CreateDirectory(path_.Append("ac"));
    file_util::CreateDirectory(path_.Append("battery"));
    map<string, string> values;
    values["ac/online"] = on_ac ? kOnline : kOffline;
    values["ac/type"] = kACType;
    values["battery/type"] = kBatteryType;
    values["battery/present"] = kPresent;
    if (report_charge) {
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
  EXPECT_FALSE(power_status.battery_is_present);
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
  EXPECT_FALSE(power_status.battery_is_present);
}

// Test battery charging status.
TEST_F(PowerSupplyTest, TestCharging) {
  WriteDefaultValues(true, true);
  power_supply_->Init();
  PowerStatus power_status;
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_TRUE(power_status.line_power_on);
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToFull, power_status.battery_time_to_full);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
}

// Test battery discharging status.  Test both positive and negative current
// values.
TEST_F(PowerSupplyTest, TestDischarging) {
  WriteDefaultValues(false, true);
  power_supply_->Init();
  PowerStatus power_status;
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);

  WriteValue("battery/current_now", base::IntToString(-kCurrentNowInt));
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_FALSE(power_status.line_power_on);
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
}

// Test battery reporting energy instead of charge.
TEST_F(PowerSupplyTest, TestEnergyDischarging) {
  WriteDefaultValues(false, false);
  power_supply_->Init();
  PowerStatus power_status;
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);

  WriteValue("battery/power_now", base::IntToString(-kPowerNowInt));
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_FALSE(power_status.line_power_on);
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
}

namespace {

// Used for supplying long sequences of simulated power supply readings.
struct PowerSupplyValues {
  double charge;
  double current;
  int64 time_to_empty;
};

}  // namespace

TEST_F(PowerSupplyTest, TestDischargingWithHysteresis) {
  // Poll the battery once per second.
  const int kPollIntervalMs = 1000;

  file_util::CreateDirectory(path_.Append("ac"));
  file_util::CreateDirectory(path_.Append("battery"));
  // List of power values to be simulated at one-second intervals.
  // The order is: { charge, current, time remaining }
  // Note that the charge values go all over the place.  They're not meant to be
  // realistic.  Instead, the charge and current values are meant to simulate a
  // sequence of calculated battery times, to test the hysteresis mechanism of
  // the power supply reader.
  const PowerSupplyValues kValueTable[] = {
    // 0-4: First establish a baseline acceptable time.
    { 10.000000, 100, 360 },
    { 13.333333, 100, 480 },
    { 10.000000, 100, 360 },
    { 13.333333, 100, 480 },
    { 10.000000, 100, 360 },
    // 5-7: Baseline established at 360, vary it slightly.
    {  9.972222, 100, 359 },
    {  9.944444, 100, 358 },
    {  9.916667, 100, 357 },
    // 8-10: Increase the time beyond the allowed variance.
    { 11.111111, 100, 363 },
    { 11.388889, 100, 362 },
    { 11.666667, 100, 361 },
    // 11-13: New baseline established at 440.
    { 12.222222, 100, 440 },
    { 12.194444, 100, 439 },
    { 12.166667, 100, 438 },
    // 14-17: Fix time at 445, while the upper bound drops gradually.
    { 12.361111, 100, 445 },
    { 12.361111, 100, 445 },
    { 12.361111, 100, 444 },
    { 12.361111, 100, 443 },
    // 18-21: New baseline established at 445.
    { 12.361111, 100, 445 },
    { 12.361111, 100, 445 },
    { 12.361111, 100, 445 },
    { 12.361111, 100, 445 },
    // 22-24: Increase the calculated time beyond the allowed variance.
    { 16.666667, 100, 450 },
    { 16.666667, 100, 449 },
    { 16.666667, 100, 448 },
    // 25-27: Bring it back down before it becomes the new baseline.
    { 12.222222, 100, 440 },
    { 12.222222, 100, 440 },
    { 12.222222, 100, 440 },
    // 28-32: Drop the calculated time below lower bound.  Eventually the lower
    // bound will drop so that the calculated time becomes acceptable.
    { 11.777778, 100, 426 },
    { 11.777778, 100, 425 },
    { 11.777778, 100, 424 },
    { 11.777778, 100, 424 },
    { 11.777778, 100, 424 },
    // 33-36: Drop the calculated time to 400 and hold it until it becomes the
    // new baseline.
    { 11.111111, 100, 421 },
    { 11.111111, 100, 420 },
    { 11.111111, 100, 419 },
    { 11.111111, 100, 400 },
    // 37-39: Let it keep dropping from 400.
    { 11.111111, 100, 400 },
    { 10.972222, 100, 395 },
    { 10.972222, 100, 395 },
    // 40-42: Drop to 350, should be clipped at lower bound.
    {  9.722222, 100, 388 },
    {  9.722222, 100, 387 },
    {  9.722222, 100, 386 },
    // 43-45: Bring it back up to 390.
    { 10.833333, 100, 390 },
    { 10.833333, 100, 390 },
    { 10.833333, 100, 390 },
  };
  WriteDefaultValues(false, true);
  map<string, string> values;
  values["battery/charge_now"] = base::IntToString(kValueTable[0].charge);
  values["battery/current_now"] = base::IntToString(kValueTable[0].current);
  WriteValues(values);

  base::TimeTicks start_time = base::TimeTicks::Now();
  test_api_->set_current_time(start_time);
  power_supply_->Init();

  int num_entries = sizeof(kValueTable) / sizeof(kValueTable[0]);
  for (int i = 0; i < num_entries; ++i) {
    // Write charge and current values to simulated sysfs.
    const PowerSupplyValues& current_values = kValueTable[i];
    map<string, string> value_map;
    value_map["battery/charge_now"] =
        base::IntToString(ScaleDouble(current_values.charge));
    value_map["battery/current_now"] =
        base::IntToString(ScaleDouble(current_values.current));
    WriteValues(value_map);

    // Override the default hysteresis time with something short, so the test
    // doesn't take forever.  In this case, we want 4 seconds.
    power_supply_->hysteresis_time_ = base::TimeDelta::FromSeconds(4);

    test_api_->set_current_time(start_time +
        base::TimeDelta::FromMilliseconds(kPollIntervalMs * (i + 1)));
    EXPECT_TRUE(power_supply_->RefreshImmediately());
    EXPECT_EQ(current_values.time_to_empty,
              power_supply_->power_status().battery_time_to_empty)
        << "Remaining time mismatch found at index " << i << "; charge: "
        << current_values.charge << "; current: " << current_values.current;
  }
}

// Test battery discharge while suspending and resuming.
TEST_F(PowerSupplyTest, TestDischargingWithSuspendResume) {
  WriteDefaultValues(false, true);

  base::TimeTicks current_time = base::TimeTicks::Now();
  test_api_->set_current_time(current_time);
  power_supply_->Init();
  power_supply_->hysteresis_time_ = base::TimeDelta::FromSeconds(4);

  PowerStatus power_status;
  ASSERT_TRUE(UpdateStatus(&power_status));
  current_time += base::TimeDelta::FromSeconds(6);
  test_api_->set_current_time(current_time);
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
  // Store initial remaining battery time reading.
  int64 time_to_empty = power_status.battery_time_to_empty;

  // Suspend, reduce the current, and resume.
  power_supply_->SetSuspended(true);
  WriteValue("battery/current_now", base::IntToString(kCurrentNowInt / 10));
  current_time += base::TimeDelta::FromSeconds(8);
  test_api_->set_current_time(current_time);
  // Verify that the time remaining hasn't increased dramatically.
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_NEAR(power_status.battery_time_to_empty, time_to_empty,
              time_to_empty * power_supply_->acceptable_variance_);
  current_time += base::TimeDelta::FromSeconds(2);
  test_api_->set_current_time(current_time);
  // Restore normal current reading.
  WriteValue("battery/current_now", base::IntToString(kCurrentNowInt));
  ASSERT_TRUE(UpdateStatus(&power_status));
  EXPECT_NEAR(power_status.battery_time_to_empty, time_to_empty, 30);
}

TEST_F(PowerSupplyTest, PollDelays) {
  WriteDefaultValues(true, true);

  const base::TimeDelta kPollDelay = base::TimeDelta::FromSeconds(30);
  const base::TimeDelta kShortPollDelay = base::TimeDelta::FromSeconds(5);
  const base::TimeDelta kShortPollDelayPlusSlack = kShortPollDelay +
      base::TimeDelta::FromMilliseconds(PowerSupply::kCalculateBatterySlackMs);

  prefs_.SetInt64(kBatteryPollIntervalPref, kPollDelay.InMilliseconds());
  prefs_.SetInt64(kBatteryPollShortIntervalPref,
                   kShortPollDelay.InMilliseconds());

  base::TimeTicks current_time = base::TimeTicks::FromInternalValue(1000);
  test_api_->set_current_time(current_time);
  power_supply_->Init();

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
  test_api_->set_current_time(current_time);
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
  test_api_->set_current_time(current_time);
  WriteValue("ac/online", kOffline);
  power_supply_->SetSuspended(false);
  status = power_supply_->power_status();
  EXPECT_FALSE(status.line_power_on);
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(kShortPollDelayPlusSlack.InMilliseconds(),
            test_api_->current_poll_delay().InMilliseconds());

  // Check that the updated times are returned after a delay.
  current_time += kShortPollDelayPlusSlack;
  test_api_->set_current_time(current_time);
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
}

TEST_F(PowerSupplyTest, UpdateAveragedTimes) {
  // Start out with the battery 50% full and a current such that it'll take
  // an hour to charge fully.
  WriteDefaultValues(true, true);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", 0.5);
  WriteDoubleValue("battery/current_now", 0.5);
  base::TimeTicks now = base::TimeTicks::Now();
  test_api_->set_current_time(now);
  power_supply_->Init();

  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty);
  EXPECT_EQ(0, status.averaged_battery_time_to_empty);
  EXPECT_EQ(3600, status.battery_time_to_full);
  EXPECT_EQ(0, status.averaged_battery_time_to_full);

  // Let half an hour pass and report that the battery is 75% full.
  now += base::TimeDelta::FromMinutes(30);
  test_api_->set_current_time(now);
  WriteDoubleValue("battery/charge_now", 0.75);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty);
  EXPECT_EQ(0, status.averaged_battery_time_to_empty);
  EXPECT_EQ(1800, status.battery_time_to_full);
  EXPECT_EQ(1800, status.averaged_battery_time_to_full);

  // Fifteen minutes later, report that the battery is continuing to fill.
  // The time-to-full field should reflect the latest reading, but the
  // averaged-time-to-full should be halfway between the last reading and
  // the latest one.
  now += base::TimeDelta::FromMinutes(15);
  test_api_->set_current_time(now);
  WriteDoubleValue("battery/charge_now", 0.875);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(0, status.battery_time_to_empty);
  EXPECT_EQ(0, status.averaged_battery_time_to_empty);
  EXPECT_EQ(900, status.battery_time_to_full);
  EXPECT_EQ((1800 + 900) / 2, status.averaged_battery_time_to_full);

  // Thirty minutes later, report that the battery is at 50% and the system
  // is currently on battery power, with a current such that it'll be empty
  // after an hour.
  now += base::TimeDelta::FromMinutes(30);
  test_api_->set_current_time(now);
  WriteValue("ac/online", kOffline);
  WriteDoubleValue("battery/charge_now", 0.5);
  WriteDoubleValue("battery/current_now", -0.5);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(3600, status.battery_time_to_empty);
  EXPECT_EQ(3600 - kLowBatteryShutdownTimeSec,
            status.averaged_battery_time_to_empty);
  EXPECT_EQ(0, status.battery_time_to_full);
  EXPECT_EQ(0, status.averaged_battery_time_to_full);

  // After thirty more minutes, report that the battery is at 25%.
  now += base::TimeDelta::FromMinutes(30);
  test_api_->set_current_time(now);
  WriteDoubleValue("battery/charge_now", 0.25);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.is_calculating_battery_time);
  EXPECT_EQ(1800, status.battery_time_to_empty);
  EXPECT_EQ((1800 + 3600 - 2 * kLowBatteryShutdownTimeSec) / 2,
            status.averaged_battery_time_to_empty);
  EXPECT_EQ(0, status.battery_time_to_full);
  EXPECT_EQ(0, status.averaged_battery_time_to_full);

  // Give a bogusly-low current and check that the average time is left unset.
  now += base::TimeDelta::FromMinutes(10);
  test_api_->set_current_time(now);
  WriteDoubleValue("battery/charge_now", 0.20);
  WriteDoubleValue("battery/current_now", -0.0002);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(1000 * 3600, status.battery_time_to_empty);
  EXPECT_EQ(0, status.averaged_battery_time_to_empty);
}

TEST_F(PowerSupplyTest, DisplayBatteryPercent) {
  // Start out with a full battery on AC power.
  WriteDefaultValues(true, true);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", 1.0);
  WriteDoubleValue("battery/current_now", 0.0);
  base::TimeTicks now = base::TimeTicks::Now();
  test_api_->set_current_time(now);
  power_supply_->Init();

  // 100% should be reported both on AC and on battery.
  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);
  WriteValue("ac/online", kOffline);
  WriteDoubleValue("battery/current_now", -1.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);

  // Decrease the battery charge.  After half of the pinning interval
  // elapses, 100% should still be reported.
  WriteDoubleValue("battery/charge_now", 0.95);
  now += base::TimeDelta::FromMilliseconds(kBatteryPercentPinMs / 2);
  test_api_->set_current_time(now);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);

  // The display percentage should still be at 100% at the end of the
  // pinning interval.
  now += base::TimeDelta::FromMilliseconds(kBatteryPercentPinMs / 2);
  test_api_->set_current_time(now);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);

  // Over the tapering interval, the display percentage should be linearly
  // scaled down to the actual percentage.
  now += base::TimeDelta::FromMilliseconds(kBatteryPercentTaperMs / 2);
  test_api_->set_current_time(now);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(97.5, status.display_battery_percentage);

  // The actual percentage should be reported after tapering is complete.
  now += base::TimeDelta::FromMilliseconds(kBatteryPercentTaperMs / 2);
  test_api_->set_current_time(now);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(95.0, status.display_battery_percentage);

  now += base::TimeDelta::FromMinutes(1);
  test_api_->set_current_time(now);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(95.0, status.display_battery_percentage);

  WriteDoubleValue("battery/charge_now", 0.9);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(90.0, status.display_battery_percentage);

  // Switch to AC and then back to power and check that the level isn't
  // pinned to 100% anymore.
  WriteValue("ac/online", kOnline);
  WriteDoubleValue("battery/current_now", 1.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(90.0, status.display_battery_percentage);

  WriteValue("ac/online", kOffline);
  WriteDoubleValue("battery/current_now", -1.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(90.0, status.display_battery_percentage);

  WriteDoubleValue("battery/charge_now", 0.85);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(85.0, status.display_battery_percentage);

  // After charging to 100%, going to battery power, and suspending and
  // resuming, the actual level should be reported immediately.
  WriteValue("ac/online", kOnline);
  WriteDoubleValue("battery/charge_now", 1.0);
  WriteDoubleValue("battery/current_now", 0.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);

  WriteValue("ac/online", kOffline);
  WriteDoubleValue("battery/current_now", -1.0);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);

  WriteDoubleValue("battery/charge_now", 0.95);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_DOUBLE_EQ(100.0, status.display_battery_percentage);

  power_supply_->SetSuspended(true);
  power_supply_->SetSuspended(false);
  status = power_supply_->power_status();
  EXPECT_DOUBLE_EQ(95.0, status.display_battery_percentage);
}

TEST_F(PowerSupplyTest, CheckForLowBattery) {
  double kShutdownCharge = kLowBatteryShutdownTimeSec / 3600.0;
  WriteDefaultValues(false, true);
  WriteDoubleValue("battery/charge_full", 1.0);
  WriteDoubleValue("battery/charge_now", kShutdownCharge + 0.01);
  WriteDoubleValue("battery/current_now", -1.0);
  power_supply_->Init();

  PowerStatus status;
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_FALSE(status.battery_below_shutdown_threshold);

  WriteDoubleValue("battery/charge_now", kShutdownCharge - 0.01);
  ASSERT_TRUE(UpdateStatus(&status));
  EXPECT_TRUE(status.battery_below_shutdown_threshold);
}

}  // namespace system
}  // namespace power_manager
