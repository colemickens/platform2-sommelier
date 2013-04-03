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
const int64 kTimeToFull = lround(3600. * (kChargeFull - kChargeNow) /
                                     kCurrentNow);
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

}  // namespace

class PowerSupplyTest : public ::testing::Test {
 public:
  PowerSupplyTest() {}

  virtual void SetUp() {
    temp_dir_generator_.reset(new base::ScopedTempDir());
    ASSERT_TRUE(temp_dir_generator_->CreateUniqueTempDir());
    EXPECT_TRUE(temp_dir_generator_->IsValid());
    path_ = temp_dir_generator_->path();
    power_supply_.reset(new PowerSupply(path_, &prefs_));
    test_api_.reset(new PowerSupply::TestApi(power_supply_.get()));
  }

 protected:
  // Writes the entries in |value_map| to |path_|.  Keys are relative
  // filenames and values are the values to be written.
  void WriteValues(const map<string, string>& values) {
    for (map<string, string>::const_iterator iter = values.begin();
         iter != values.end(); ++iter) {
      CHECK(file_util::WriteFile(path_.Append(iter->first),
                                 iter->second.c_str(),
                                 iter->second.length()));
    }
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
  EXPECT_TRUE(power_supply_->RefreshImmediately());
  PowerStatus power_status = power_supply_->power_status();
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
  EXPECT_TRUE(power_supply_->RefreshImmediately());
  PowerStatus power_status = power_supply_->power_status();
  EXPECT_TRUE(power_status.line_power_on);
  EXPECT_FALSE(power_status.battery_is_present);
}

// Test battery charging status.
TEST_F(PowerSupplyTest, TestCharging) {
  file_util::CreateDirectory(path_.Append("ac"));
  file_util::CreateDirectory(path_.Append("battery"));
  map<string, string> values;
  values["ac/online"] = kOnline;
  values["ac/type"] = kACType;
  values["battery/type"] = kBatteryType;
  values["battery/present"] = kPresent;
  values["battery/charge_full"] = base::IntToString(kChargeFullInt);
  values["battery/charge_full_design"] = base::IntToString(kChargeFullInt);
  values["battery/charge_now"] = base::IntToString(kChargeNowInt);
  values["battery/current_now"] = base::IntToString(kCurrentNowInt);
  values["battery/voltage_now"] = base::IntToString(kVoltageNowInt);
  values["battery/cycle_count"] = base::IntToString(kCycleCount);
  WriteValues(values);

  power_supply_->Init();
  ASSERT_TRUE(power_supply_->RefreshImmediately());
  PowerStatus power_status = power_supply_->power_status();
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
  file_util::CreateDirectory(path_.Append("ac"));
  file_util::CreateDirectory(path_.Append("battery"));
  map<string, string> values;
  values["ac/online"] = kOffline;
  values["ac/type"] = kACType;
  values["battery/type"] = kBatteryType;
  values["battery/present"] = kPresent;
  values["battery/charge_full"] = base::IntToString(kChargeFullInt);
  values["battery/charge_full_design"] = base::IntToString(kChargeFullInt);
  values["battery/charge_now"] = base::IntToString(kChargeNowInt);
  values["battery/current_now"] = base::IntToString(kCurrentNowInt);
  values["battery/voltage_now"] = base::IntToString(kVoltageNowInt);
  values["battery/cycle_count"] = base::IntToString(kCycleCount);
  WriteValues(values);

  power_supply_->Init();
  ASSERT_TRUE(power_supply_->RefreshImmediately());
  PowerStatus power_status = power_supply_->power_status();
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);

  file_util::WriteFile(path_.Append("battery/current_now"),
                       base::IntToString(-kCurrentNowInt).c_str(),
                       base::IntToString(-kCurrentNowInt).length());
  EXPECT_TRUE(power_supply_->RefreshImmediately());
  power_status = power_supply_->power_status();
  EXPECT_FALSE(power_status.line_power_on);
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
}

// Test battery reporting energy instead of charge.
TEST_F(PowerSupplyTest, TestEnergyDischarging) {
  file_util::CreateDirectory(path_.Append("ac"));
  file_util::CreateDirectory(path_.Append("battery"));
  map<string, string> values;
  values["ac/online"] = kOffline;
  values["ac/type"] = kACType;
  values["battery/type"] = kBatteryType;
  values["battery/present"] = kPresent;
  values["battery/energy_full"] = base::IntToString(kEnergyFullInt);
  values["battery/energy_full_design"] = base::IntToString(kEnergyFullInt);
  values["battery/energy_now"] = base::IntToString(kEnergyNowInt);
  values["battery/power_now"] = base::IntToString(kPowerNowInt);
  values["battery/voltage_now"] = base::IntToString(kVoltageNowInt);
  values["battery/cycle_count"] = base::IntToString(kCycleCount);
  WriteValues(values);

  power_supply_->Init();
  ASSERT_TRUE(power_supply_->RefreshImmediately());
  PowerStatus power_status = power_supply_->power_status();
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);

  file_util::WriteFile(path_.Append("battery/power_now"),
                       base::IntToString(-kPowerNowInt).c_str(),
                       base::IntToString(-kPowerNowInt).length());
  EXPECT_TRUE(power_supply_->RefreshImmediately());
  power_status = power_supply_->power_status();
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
  map<string, string> values;
  values["ac/online"] = kOffline;
  values["ac/type"] = kACType;
  values["battery/type"] = kBatteryType;
  values["battery/present"] = kPresent;
  values["battery/charge_full"] = base::IntToString(kChargeFullInt);
  values["battery/charge_full_design"] = base::IntToString(kChargeFullInt);
  values["battery/voltage_now"] = base::IntToString(kVoltageNowInt);
  values["battery/cycle_count"] = base::IntToString(kCycleCount);
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
  file_util::CreateDirectory(path_.Append("ac"));
  file_util::CreateDirectory(path_.Append("battery"));
  map<string, string> values;
  values["ac/online"] = kOffline;
  values["ac/type"] = kACType;
  values["battery/type"] = kBatteryType;
  values["battery/present"] = kPresent;
  values["battery/charge_full"] = base::IntToString(kChargeFullInt);
  values["battery/charge_full_design"] = base::IntToString(kChargeFullInt);
  values["battery/charge_now"] = base::IntToString(kChargeNowInt);
  values["battery/current_now"] = base::IntToString(kCurrentNowInt);
  values["battery/voltage_now"] = base::IntToString(kVoltageNowInt);
  values["battery/cycle_count"] = base::IntToString(kCycleCount);
  WriteValues(values);

  base::TimeTicks current_time = base::TimeTicks::Now();
  test_api_->set_current_time(current_time);
  power_supply_->Init();
  power_supply_->hysteresis_time_ = base::TimeDelta::FromSeconds(4);

  EXPECT_TRUE(power_supply_->RefreshImmediately());
  current_time += base::TimeDelta::FromSeconds(6);
  test_api_->set_current_time(current_time);
  EXPECT_TRUE(power_supply_->RefreshImmediately());
  PowerStatus power_status = power_supply_->power_status();
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
  // Store initial remaining battery time reading.
  int64 time_to_empty = power_status.battery_time_to_empty;

  // Suspend, reduce the current, and resume.
  power_supply_->SetSuspended(true);
  file_util::WriteFile(path_.Append("battery/current_now"),
                       base::IntToString(kCurrentNowInt / 10).c_str(),
                       base::IntToString(kCurrentNowInt / 10).length());
  current_time += base::TimeDelta::FromSeconds(8);
  test_api_->set_current_time(current_time);
  // Verify that the time remaining hasn't increased dramatically.
  EXPECT_TRUE(power_supply_->RefreshImmediately());
  power_status = power_supply_->power_status();
  EXPECT_NEAR(power_status.battery_time_to_empty, time_to_empty,
              time_to_empty * power_supply_->acceptable_variance_);
  current_time += base::TimeDelta::FromSeconds(2);
  test_api_->set_current_time(current_time);
  // Restore normal current reading.
  file_util::WriteFile(path_.Append("battery/current_now"),
                       base::IntToString(kCurrentNowInt).c_str(),
                       base::IntToString(kCurrentNowInt).length());
  EXPECT_TRUE(power_supply_->RefreshImmediately());
  power_status = power_supply_->power_status();
  EXPECT_NEAR(power_status.battery_time_to_empty, time_to_empty, 30);
}

TEST_F(PowerSupplyTest, PollDelays) {
  file_util::CreateDirectory(path_.Append("ac"));
  file_util::CreateDirectory(path_.Append("battery"));
  map<string, string> values;
  values["ac/online"] = kOnline;
  values["ac/type"] = kACType;
  values["battery/type"] = kBatteryType;
  values["battery/present"] = kPresent;
  values["battery/charge_full"] = base::IntToString(kChargeFullInt);
  values["battery/charge_full_design"] = base::IntToString(kChargeFullInt);
  values["battery/charge_now"] = base::IntToString(kChargeNowInt);
  values["battery/current_now"] = base::IntToString(kCurrentNowInt);
  values["battery/voltage_now"] = base::IntToString(kVoltageNowInt);
  values["battery/cycle_count"] = base::IntToString(kCycleCount);
  WriteValues(values);

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
  ASSERT_TRUE(power_supply_->RefreshImmediately());
  PowerStatus status = power_supply_->power_status();
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
  file_util::WriteFile(path_.Append("ac/online"), kOffline, strlen(kOffline));
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
  file_util::WriteFile(path_.Append("ac/online"), kOnline, strlen(kOnline));
  power_supply_->HandleUdevEvent();
  status = power_supply_->power_status();
  EXPECT_TRUE(status.line_power_on);
  EXPECT_TRUE(status.is_calculating_battery_time);
  EXPECT_EQ(kShortPollDelayPlusSlack.InMilliseconds(),
            test_api_->current_poll_delay().InMilliseconds());
}

}  // namespace system
}  // namespace power_manager
