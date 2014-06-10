// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/dark_resume_policy.h"

#include <cmath>
#include <limits>
#include <map>
#include <string>

#include <base/files/scoped_temp_dir.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/strings/string_number_conversions.h>
#include <gtest/gtest.h>

#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/power_supply.h"
#include "power_manager/powerd/system/udev_stub.h"

using std::map;
using std::string;

namespace {

const char kOnline[] = "1";
const char kOffline[] = "0";
const char kPresent[] = "1";
const char kACType[] = "Mains";
const char kBatteryType[] = "Battery";

const double kChargeFull = 2.40;

// sysfs stores doubles by multiplying them by 1000000 and storing as an int.
int ScaleDouble(double value) {
  return round(value * 1000000);
}

}  // namespace

namespace power_manager {
namespace policy {

class DarkResumePolicyTest : public ::testing::Test {
 public:
  DarkResumePolicyTest() {}

  virtual void SetUp() {
    temp_dir_generator_.reset(new base::ScopedTempDir());
    ASSERT_TRUE(temp_dir_generator_->CreateUniqueTempDir());
    ASSERT_TRUE(temp_dir_generator_->IsValid());
    path_ = temp_dir_generator_->path();
    base::CreateDirectory(path_.Append("ac"));
    base::CreateDirectory(path_.Append("battery"));
    power_supply_.reset(new system::PowerSupply);
    dark_resume_policy_.reset(new DarkResumePolicy);
  }

 protected:
  // Initializes |power_supply_| and |dark_resume_policy_|.
  void Init() {
    prefs_.SetInt64(kMaxCurrentSamplesPref, 5);
    prefs_.SetInt64(kMaxChargeSamplesPref, 5);
    power_supply_->Init(path_, &prefs_, &udev_);
    SetBattery(100.0, false);
    dark_resume_policy_->Init(power_supply_.get(), &prefs_);
  }

  void SetBattery(double charge_percent, bool ac_online) {
    map<string, string> values;
    values["ac/type"] = kACType;
    values["battery/type"] = kBatteryType;
    values["battery/present"] = kPresent;
    values["battery/charge_full"] = base::IntToString(ScaleDouble(kChargeFull));
    values["battery/charge_full_design"] =
        base::IntToString(ScaleDouble(kChargeFull));
    values["battery/charge_now"] = base::IntToString(
        ScaleDouble(charge_percent * kChargeFull / 100.0));

    if (ac_online)
      values["ac/online"] = kOnline;
    else
      values["ac/online"] = kOffline;

    for (map<string, string>::iterator iter = values.begin();
         iter != values.end();
         ++iter) {
      base::WriteFile(path_.Append(iter->first),
                      iter->second.c_str(), iter->second.length());
    }

    ASSERT_TRUE(power_supply_->RefreshImmediately());
    ASSERT_DOUBLE_EQ(charge_percent,
                     power_supply_->power_status().battery_percentage);
  }

  FakePrefs prefs_;
  scoped_ptr<base::ScopedTempDir> temp_dir_generator_;
  base::FilePath path_;
  system::UdevStub udev_;
  scoped_ptr<system::PowerSupply> power_supply_;
  scoped_ptr<DarkResumePolicy> dark_resume_policy_;
};

// Test that GetAction will return SHUTDOWN if the preferences are correct.
TEST_F(DarkResumePolicyTest, TestShutdown) {
  prefs_.SetString(kDarkResumeBatteryMarginsPref, "0.0 -1.0");
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");
  Init();
  EXPECT_EQ(DarkResumePolicy::SHUT_DOWN, dark_resume_policy_->GetAction());
}

// Test that GetAction will first return SUSPEND_FOR_DURATION then SHUT_DOWN
// after the battery charge changes and the power is unplugged.
TEST_F(DarkResumePolicyTest, TestSuspendFirst) {
  prefs_.SetString(kDarkResumeBatteryMarginsPref, "0.0 0.0");
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");
  Init();
  EXPECT_EQ(DarkResumePolicy::SUSPEND_FOR_DURATION,
            dark_resume_policy_->GetAction());

  SetBattery(50.0, false);
  EXPECT_EQ(DarkResumePolicy::SHUT_DOWN, dark_resume_policy_->GetAction());
}

// Test that state is not maintained after a user resume and that the proper
// suspend durations are returned.
TEST_F(DarkResumePolicyTest, TestUserResumes) {
  prefs_.SetString(kDarkResumeBatteryMarginsPref, "0.0 0.0\n"
                                                  "20.0 2.0\n"
                                                  "50.0 5.0\n"
                                                  "80.0 8.0");
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10\n"
                                                    "20.0 50\n"
                                                    "50.0 100\n"
                                                    "80.0 500");
  Init();
  EXPECT_EQ(DarkResumePolicy::SUSPEND_FOR_DURATION,
            dark_resume_policy_->GetAction());
  EXPECT_EQ(500, dark_resume_policy_->GetSuspendDuration().InSeconds());

  dark_resume_policy_->HandleResume();
  SetBattery(80.0, false);
  EXPECT_EQ(DarkResumePolicy::SUSPEND_FOR_DURATION,
            dark_resume_policy_->GetAction());
  EXPECT_EQ(500, dark_resume_policy_->GetSuspendDuration().InSeconds());

  dark_resume_policy_->HandleResume();
  SetBattery(50.0, false);
  EXPECT_EQ(DarkResumePolicy::SUSPEND_FOR_DURATION,
            dark_resume_policy_->GetAction());
  EXPECT_EQ(100, dark_resume_policy_->GetSuspendDuration().InSeconds());

  dark_resume_policy_->HandleResume();
  SetBattery(20.0, false);
  EXPECT_EQ(DarkResumePolicy::SUSPEND_FOR_DURATION,
            dark_resume_policy_->GetAction());
  EXPECT_EQ(50, dark_resume_policy_->GetSuspendDuration().InSeconds());

  dark_resume_policy_->HandleResume();
  SetBattery(5.0, false);
  EXPECT_EQ(DarkResumePolicy::SUSPEND_FOR_DURATION,
            dark_resume_policy_->GetAction());
  EXPECT_EQ(10, dark_resume_policy_->GetSuspendDuration().InSeconds());
}

// Check that we don't shutdown when the AC is online (regardless of battery
// life).
TEST_F(DarkResumePolicyTest, TestACOnline) {
  prefs_.SetString(kDarkResumeBatteryMarginsPref, "0.0 0.0");
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");
  Init();
  EXPECT_EQ(DarkResumePolicy::SUSPEND_FOR_DURATION,
            dark_resume_policy_->GetAction());

  SetBattery(50.0, true);
  EXPECT_EQ(DarkResumePolicy::SUSPEND_FOR_DURATION,
            dark_resume_policy_->GetAction());
}

// Check that setting the pref disable_dark_resume to 1 disables dark resume and
// that setting it to 0 enables it.
TEST_F(DarkResumePolicyTest, TestDisable) {
  prefs_.SetInt64(kDisableDarkResumePref, 1);
  prefs_.SetString(kDarkResumeBatteryMarginsPref, "0.0 0.0");
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");
  Init();
  EXPECT_EQ(DarkResumePolicy::SUSPEND_INDEFINITELY,
            dark_resume_policy_->GetAction());
}

TEST_F(DarkResumePolicyTest, TestEnable) {
  prefs_.SetInt64(kDisableDarkResumePref, 0);
  prefs_.SetString(kDarkResumeBatteryMarginsPref, "0.0 0.0");
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");
  Init();
  EXPECT_EQ(DarkResumePolicy::SUSPEND_FOR_DURATION,
            dark_resume_policy_->GetAction());
}

}  // namespace policy
}  // namespace power_manager
