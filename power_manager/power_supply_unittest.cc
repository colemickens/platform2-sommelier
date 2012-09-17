// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <string>

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "power_manager/power_supply.h"
#include "power_manager/signal_callback.h"

using std::map;
using std::string;

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

namespace power_manager {

class PowerSupplyTest : public ::testing::Test {
 public:
  PowerSupplyTest() {}

  virtual void SetUp() {
    // Create a temporary directory for the test files
    temp_dir_generator_.reset(new ScopedTempDir());
    ASSERT_TRUE(temp_dir_generator_->CreateUniqueTempDir());
    EXPECT_TRUE(temp_dir_generator_->IsValid());
    // Initialize the file tagger
    power_supply_.reset(new PowerSupply(temp_dir_generator_->path(), NULL));
  }

 protected:
  scoped_ptr<PowerSupply> power_supply_;
  scoped_ptr<ScopedTempDir> temp_dir_generator_;
};

// Test that when false is passed in the correct entry in the data structure is
// set.
TEST_F(PowerSupplyTest, IsNotCalculatingTime) {
  power_supply_->Init();
  PowerStatus power_status;
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_FALSE(power_status.is_calculating_battery_time);
}

// Test that when true is passed in the correct entry in the data structure is
// set.
TEST_F(PowerSupplyTest, IsCalculatingTime) {
  power_supply_->Init();
  PowerStatus power_status;
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, true));
  EXPECT_TRUE(power_status.is_calculating_battery_time);
}

// Test system without power supply sysfs (e.g. virtual machine).
TEST_F(PowerSupplyTest, TestNoPowerSupplySysfs) {
  power_supply_->Init();
  PowerStatus power_status;
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  // In absence of power supply sysfs, default assumption is line power on, no
  // battery present.
  EXPECT_TRUE(power_status.line_power_on);
  EXPECT_FALSE(power_status.battery_is_present);
}

// Test line power without battery.
TEST_F(PowerSupplyTest, TestNoBattery) {
  FilePath path = temp_dir_generator_->path();
  file_util::CreateDirectory(path.Append("ac"));
  map<string, string> values;
  values["ac/online"] = kOnline;
  values["ac/type"] = kACType;

  for (map<string, string>::iterator iter = values.begin();
       iter != values.end();
       ++iter)
    file_util::WriteFile(path.Append(iter->first),
                         iter->second.c_str(),
                         iter->second.length());
  power_supply_->Init();
  PowerStatus power_status;
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_TRUE(power_status.line_power_on);
  EXPECT_FALSE(power_status.battery_is_present);
}

// Test battery charging status.
TEST_F(PowerSupplyTest, TestCharging) {
  FilePath path = temp_dir_generator_->path();
  file_util::CreateDirectory(path.Append("ac"));
  file_util::CreateDirectory(path.Append("battery"));
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

  for (map<string, string>::iterator iter = values.begin();
       iter != values.end();
       ++iter)
    file_util::WriteFile(path.Append(iter->first),
                         iter->second.c_str(),
                         iter->second.length());
  power_supply_->Init();
  PowerStatus power_status;
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
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
  FilePath path = temp_dir_generator_->path();
  file_util::CreateDirectory(path.Append("ac"));
  file_util::CreateDirectory(path.Append("battery"));
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

  for (map<string, string>::iterator iter = values.begin();
       iter != values.end();
       ++iter)
    file_util::WriteFile(path.Append(iter->first),
                         iter->second.c_str(),
                         iter->second.length());
  power_supply_->Init();
  PowerStatus power_status;
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);

  file_util::WriteFile(path.Append("battery/current_now"),
                       base::IntToString(-kCurrentNowInt).c_str(),
                       base::IntToString(-kCurrentNowInt).length());
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_FALSE(power_status.line_power_on);
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
}

// Test battery reporting energy instead of charge.
TEST_F(PowerSupplyTest, TestEnergyDischarging) {
  FilePath path = temp_dir_generator_->path();
  file_util::CreateDirectory(path.Append("ac"));
  file_util::CreateDirectory(path.Append("battery"));
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

  for (map<string, string>::iterator iter = values.begin();
       iter != values.end();
       ++iter)
    file_util::WriteFile(path.Append(iter->first),
                         iter->second.c_str(),
                         iter->second.length());
  power_supply_->Init();
  PowerStatus power_status;
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);

  file_util::WriteFile(path.Append("battery/power_now"),
                       base::IntToString(-kPowerNowInt).c_str(),
                       base::IntToString(-kPowerNowInt).length());
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
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

struct HysteresisTestData {
  PowerSupply* power_supply;
  PowerSupplyValues* values;
  FilePath* path;
  int index;
  int time;
};

// Poll the battery once per second.
const int kPollIntervalMs = 1000;

// This is used for mocking out base::Time::Now().  This setup allows a test
// function to provide the desired time reading by setting current_time.  The
// function GetCurrentTime() is a callback used for returning it.  It is passed
// to PowerSupply to be used instead of base::Time::Now();
base::Time start_time;
base::Time current_time;
base::Time GetCurrentTime() {
  return current_time;
}

gboolean HysteresisTestLoop(gpointer data) {
  HysteresisTestData* test_data = static_cast<HysteresisTestData*>(data);
  PowerSupplyValues* values = test_data->values;
  // Write charge and current values to simulated sysfs.
  map<string, string> value_map;
  value_map["battery/charge_now"] =
    base::IntToString(ScaleDouble(values->charge));
  value_map["battery/current_now"] =
    base::IntToString(ScaleDouble(values->current));
  for (map<string, string>::iterator iter = value_map.begin();
       iter != value_map.end();
       ++iter)
    file_util::WriteFile(test_data->path->Append(iter->first),
                         iter->second.c_str(),
                         iter->second.length());

  // Tell GetCurrentTime() to return the simulated time.
  current_time = start_time +
                 base::TimeDelta::FromMilliseconds(test_data->time);
  PowerStatus power_status;
  EXPECT_TRUE(test_data->power_supply->GetPowerStatus(&power_status, false));
  EXPECT_EQ(values->time_to_empty, power_status.battery_time_to_empty);
  LOG_IF(INFO, values->time_to_empty !=
      power_status.battery_time_to_empty) << "Remaining time mismatch found "
      << "at index " << test_data->index
      << "; charge: " << values->charge
      << "; current: " << values->current;

  delete test_data;
  return false;
}

}  // namespace

TEST_F(PowerSupplyTest, TestDischargingWithHysteresis) {
  FilePath path = temp_dir_generator_->path();
  file_util::CreateDirectory(path.Append("ac"));
  file_util::CreateDirectory(path.Append("battery"));
  // List of power values to be simulated at one-second intervals.
  // The order is: { charge, current, time remaining }
  // Note that the charge values go all over the place.  They're not meant to be
  // realistic.  Instead, the charge and current values are meant to simulate a
  // sequence of calculated battery times, to test the hysteresis mechanism of
  // the power supply reader.
  PowerSupplyValues value_table[] = {
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
  values["battery/charge_now"] = base::IntToString(value_table[0].charge);
  values["battery/current_now"] = base::IntToString(value_table[0].current);

  for (map<string, string>::iterator iter = values.begin();
       iter != values.end();
       ++iter)
    file_util::WriteFile(path.Append(iter->first),
                         iter->second.c_str(),
                         iter->second.length());

  // Initialize the power supply object the first time around.
  power_supply_->Init();
  // PowerSupply should use custom time function instead of base::Time::Now().
  power_supply_->time_now_func = GetCurrentTime;
  // Save the starting time so it can be used for custom time readings.
  start_time = base::Time::Now();

  int num_entries = sizeof(value_table) / sizeof(value_table[0]);
  for (int i = 0; i < num_entries; ++i) {
    HysteresisTestData* data = new HysteresisTestData;
    CHECK(data);
    data->power_supply = power_supply_.get();
    data->values = &value_table[i];
    data->path = &path;
    data->index = i;
    data->time = kPollIntervalMs * (i + 1);
    // Override the default hysteresis time with something short, so the test
    // doesn't take forever.  In this case, we want 4 seconds.
    power_supply_->hysteresis_time_ = base::TimeDelta::FromSeconds(4);
    HysteresisTestLoop(data);
  }
}

// Test battery discharge while suspending and resuming.
TEST_F(PowerSupplyTest, TestDischargingWithSuspendResume) {
  FilePath path = temp_dir_generator_->path();
  file_util::CreateDirectory(path.Append("ac"));
  file_util::CreateDirectory(path.Append("battery"));
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

  for (map<string, string>::iterator iter = values.begin();
       iter != values.end();
       ++iter)
    file_util::WriteFile(path.Append(iter->first),
                         iter->second.c_str(),
                         iter->second.length());
  power_supply_->Init();
  power_supply_->hysteresis_time_ = base::TimeDelta::FromSeconds(4);
  power_supply_->time_now_func = GetCurrentTime;

  PowerStatus power_status;
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  current_time += base::TimeDelta::FromSeconds(6);
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
  // Store initial remaining battery time reading.
  int64 time_to_empty = power_status.battery_time_to_empty;

  // Suspend, reduce the current, and resume.
  power_supply_->SetSuspendState(true);
  file_util::WriteFile(path.Append("battery/current_now"),
                       base::IntToString(kCurrentNowInt / 10).c_str(),
                       base::IntToString(kCurrentNowInt / 10).length());
  current_time += base::TimeDelta::FromSeconds(8);
  // Verify that the time remaining hasn't increased dramatically.
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_NEAR(power_status.battery_time_to_empty, time_to_empty,
              time_to_empty * power_supply_->acceptable_variance_);
  current_time += base::TimeDelta::FromSeconds(2);
  // Restore normal current reading.
  file_util::WriteFile(path.Append("battery/current_now"),
                       base::IntToString(kCurrentNowInt).c_str(),
                       base::IntToString(kCurrentNowInt).length());
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_NEAR(power_status.battery_time_to_empty, time_to_empty, 30);
}

}  // namespace power_manager
