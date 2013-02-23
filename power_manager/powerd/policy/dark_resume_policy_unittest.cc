// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <map>
#include <string>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/policy/dark_resume_policy.h"
#include "power_manager/powerd/power_supply.h"

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

// sysfs stores doubles by multiplying them by 1000000 and storing as an int.
int ScaleDouble(double value) {
  return round(value * 1000000);
}

void SetBattery(base::FilePath& path,
                double charge_percent,
                bool ac_online) {
  map<string, string> values;
  values["ac/type"] = kACType;
  values["battery/type"] = kBatteryType;
  values["battery/present"] = kPresent;
  values["battery/charge_full"] = base::IntToString(ScaleDouble(kChargeFull));
  values["battery/charge_full_design"] =
      base::IntToString(ScaleDouble(kChargeFull));
  values["battery/charge_now"] = base::IntToString(ScaleDouble(charge_percent *
                                                               kChargeFull /
                                                               100.0));

  if (ac_online)
    values["ac/online"] = kOnline;
  else
    values["ac/online"] = kOffline;

  for (map<string, string>::iterator iter = values.begin();
       iter != values.end();
       ++iter) {
    file_util::WriteFile(path.Append(iter->first),
                         iter->second.c_str(),
                         iter->second.length());
  }
}

}  // namespace

namespace power_manager {
namespace policy {

class DarkResumePolicyTest : public ::testing::Test {
 public:
  DarkResumePolicyTest() {}

  virtual void SetUp() {
    // Create a temporary directory for the test files.
    temp_dir_generator_.reset(new base::ScopedTempDir());
    ASSERT_TRUE(temp_dir_generator_->CreateUniqueTempDir());
    ASSERT_TRUE(temp_dir_generator_->IsValid());
    path_ = temp_dir_generator_->path();
    file_util::CreateDirectory(path_.Append("ac"));
    file_util::CreateDirectory(path_.Append("battery"));
    power_supply_.reset(new PowerSupply(path_, NULL));
    dark_resume_policy_.reset(new DarkResumePolicy(power_supply_.get(),
                                                   &prefs_));
  }

 protected:
  FakePrefs prefs_;
  scoped_ptr<base::ScopedTempDir> temp_dir_generator_;
  base::FilePath path_;
  scoped_ptr<PowerSupply> power_supply_;
  scoped_ptr<DarkResumePolicy> dark_resume_policy_;
};

// Test that GetAction will return SHUTDOWN if the preferences are correct.
TEST_F(DarkResumePolicyTest, TestShutdown) {
  PowerStatus power_status;
  SetBattery(path_, 100.0, false);

  prefs_.SetString(kDarkResumeBatteryMarginsPref, "0.0 -1.0");
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");
  power_supply_->Init();
  dark_resume_policy_->Init();

  // GetPowerStatus needs to work for GetAction to work.
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_DOUBLE_EQ(100.0, power_status.battery_percentage);
  EXPECT_EQ(dark_resume_policy_->GetAction(), DarkResumePolicy::SHUT_DOWN);
}

// Test that GetAction will first return SUSPEND_FOR_DURATION then SHUT_DOWN
// after the battery charge changes and the power is unplugged.
TEST_F(DarkResumePolicyTest, TestSuspendFirst) {
  PowerStatus power_status;
  SetBattery(path_, 100.0, false);

  prefs_.SetString(kDarkResumeBatteryMarginsPref, "0.0 0.0");
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");
  power_supply_->Init();
  dark_resume_policy_->Init();

  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_DOUBLE_EQ(100.0, power_status.battery_percentage);
  EXPECT_EQ(dark_resume_policy_->GetAction(),
      DarkResumePolicy::SUSPEND_FOR_DURATION);

  SetBattery(path_, 50.0, false);
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_DOUBLE_EQ(50.0, power_status.battery_percentage);
  EXPECT_EQ(dark_resume_policy_->GetAction(), DarkResumePolicy::SHUT_DOWN);
}

// Test that state is not maintained after a user resume and that the proper
// suspend durations are returned.
TEST_F(DarkResumePolicyTest, TestUserResumes) {
  PowerStatus power_status;
  SetBattery(path_, 100.0, false);

  prefs_.SetString(kDarkResumeBatteryMarginsPref, "0.0 0.0\n"
                                                  "20.0 2.0\n"
                                                  "50.0 5.0\n"
                                                  "80.0 8.0");
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10\n"
                                                    "20.0 50\n"
                                                    "50.0 100\n"
                                                    "80.0 500");
  power_supply_->Init();
  dark_resume_policy_->Init();

  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_DOUBLE_EQ(100.0, power_status.battery_percentage);
  EXPECT_EQ(dark_resume_policy_->GetAction(),
      DarkResumePolicy::SUSPEND_FOR_DURATION);
  EXPECT_EQ(dark_resume_policy_->GetSuspendDuration().InSeconds(), 500);

  dark_resume_policy_->HandleResume();
  SetBattery(path_, 80.0, false);
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_DOUBLE_EQ(80.0, power_status.battery_percentage);
  EXPECT_EQ(dark_resume_policy_->GetAction(),
      DarkResumePolicy::SUSPEND_FOR_DURATION);
  EXPECT_EQ(dark_resume_policy_->GetSuspendDuration().InSeconds(), 500);

  dark_resume_policy_->HandleResume();
  SetBattery(path_, 50.0, false);
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_DOUBLE_EQ(50.0, power_status.battery_percentage);
  EXPECT_EQ(dark_resume_policy_->GetAction(),
      DarkResumePolicy::SUSPEND_FOR_DURATION);
  EXPECT_EQ(dark_resume_policy_->GetSuspendDuration().InSeconds(), 100);

  dark_resume_policy_->HandleResume();
  SetBattery(path_, 20.0, false);
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_DOUBLE_EQ(20.0, power_status.battery_percentage);
  EXPECT_EQ(dark_resume_policy_->GetAction(),
      DarkResumePolicy::SUSPEND_FOR_DURATION);
  EXPECT_EQ(dark_resume_policy_->GetSuspendDuration().InSeconds(), 50);

  dark_resume_policy_->HandleResume();
  SetBattery(path_, 5.0, false);
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_DOUBLE_EQ(5.0, power_status.battery_percentage);
  EXPECT_EQ(dark_resume_policy_->GetAction(),
      DarkResumePolicy::SUSPEND_FOR_DURATION);
  EXPECT_EQ(dark_resume_policy_->GetSuspendDuration().InSeconds(), 10);
}

// Check that we don't shutdown when the AC is online (regardless of battery
// life).
TEST_F(DarkResumePolicyTest, TestACOnline) {
  PowerStatus power_status;
  SetBattery(path_, 100.0, false);

  prefs_.SetString(kDarkResumeBatteryMarginsPref, "0.0 0.0");
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");
  power_supply_->Init();
  dark_resume_policy_->Init();

  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_DOUBLE_EQ(100.0, power_status.battery_percentage);
  EXPECT_EQ(dark_resume_policy_->GetAction(),
      DarkResumePolicy::SUSPEND_FOR_DURATION);

  SetBattery(path_, 50.0, true);
  EXPECT_TRUE(power_supply_->GetPowerStatus(&power_status, false));
  EXPECT_DOUBLE_EQ(50.0, power_status.battery_percentage);
  EXPECT_EQ(dark_resume_policy_->GetAction(),
      DarkResumePolicy::SUSPEND_FOR_DURATION);
}

}  // namespace policy
}  // namespace power_manager
