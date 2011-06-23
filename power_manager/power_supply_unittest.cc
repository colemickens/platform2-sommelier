// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <X11/extensions/XTest.h>

#include <cmath>
#include <map>
#include <string>

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "cros/chromeos_power.h"
#include "power_manager/power_supply.h"

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

const double kEnergyNow = kChargeNow * kVoltageNow;
const double kEnergyRate = kCurrentNow * kVoltageNow;
const double kPercentage = 100. * kChargeNow / kChargeFull;
const int64 kTimeToFull = 3600 * (kChargeFull - kChargeNow) / kCurrentNow;
const int64 kTimeToEmpty = 3600 * (kChargeNow) / kCurrentNow;

// sysfs stores doubles by multiplying them by 1000000 and storing as an int.
const int kDoubleScaleFactor = 1000000;
const int kChargeFullInt = kChargeFull * kDoubleScaleFactor;
const int kChargeNowInt = kChargeNow * kDoubleScaleFactor;
const int kCurrentNowInt = kCurrentNow * kDoubleScaleFactor;
const int kVoltageNowInt = kVoltageNow * kDoubleScaleFactor;

const int kCycleCount = 10000;

}; // namespace

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
    power_supply_.reset(new PowerSupply(temp_dir_generator_->path()));
  }

 protected:
  scoped_ptr<PowerSupply> power_supply_;
  scoped_ptr<ScopedTempDir> temp_dir_generator_;
};

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
  chromeos::PowerStatus power_status;
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status));
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
  chromeos::PowerStatus power_status;
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status));
  EXPECT_TRUE(power_status.line_power_on);
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
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
  chromeos::PowerStatus power_status;
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status));
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);

  file_util::WriteFile(path.Append("battery/current_now"),
                       base::IntToString(-kCurrentNowInt).c_str(),
                       base::IntToString(-kCurrentNowInt).length());
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status));
  EXPECT_FALSE(power_status.line_power_on);
  EXPECT_TRUE(power_status.battery_is_present);
  EXPECT_DOUBLE_EQ(kEnergyNow, power_status.battery_energy);
  EXPECT_DOUBLE_EQ(kEnergyRate, power_status.battery_energy_rate);
  EXPECT_DOUBLE_EQ(kTimeToEmpty, power_status.battery_time_to_empty);
  EXPECT_DOUBLE_EQ(kPercentage, power_status.battery_percentage);
}

}  // namespace power_manager
