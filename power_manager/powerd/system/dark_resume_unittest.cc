// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/dark_resume.h"

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/power_supply_stub.h"

namespace power_manager {
namespace system {

class DarkResumeTest : public ::testing::Test {
 public:
  DarkResumeTest() : dark_resume_(new DarkResume) {
    CHECK(temp_dir_.CreateUniqueTempDir());
    CHECK(temp_dir_.IsValid());
    state_path_ = temp_dir_.path().Append("dark_resume_state");
  }

 protected:
  // Initializes |dark_resume_|.
  void Init() {
    WriteDarkResumeState(false);
    SetBattery(100.0, false);
    dark_resume_->set_dark_resume_state_path_for_testing(state_path_);
    dark_resume_->Init(&power_supply_, &prefs_);
  }

  // Writes the passed-in dark resume state to |state_path_|.
  void WriteDarkResumeState(bool in_dark_resume) {
    ASSERT_EQ(1, base::WriteFile(state_path_, in_dark_resume ? "1" : "0", 1));
  }

  // Updates the status returned by |power_supply_|.
  void SetBattery(double charge_percent, bool ac_online) {
    PowerStatus status;
    status.battery_percentage = charge_percent;
    status.line_power_on = ac_online;
    power_supply_.set_status(status);
  }

  // Returns the contents of |path|, or an empty string if the file doesn't
  // exist.
  std::string ReadFile(const base::FilePath& path) {
    std::string value;
    base::ReadFileToString(path, &value);
    return value;
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath state_path_;
  FakePrefs prefs_;
  PowerSupplyStub power_supply_;
  scoped_ptr<DarkResume> dark_resume_;
};

TEST_F(DarkResumeTest, SuspendAndShutDown) {
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");
  Init();

  // When suspending from a non-dark-resume state, the system shouldn't shut
  // down.
  WriteDarkResumeState(false);
  SetBattery(60.0, false);
  DarkResumeInterface::Action action;
  base::TimeDelta suspend_duration;
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(10, suspend_duration.InSeconds());
  EXPECT_FALSE(dark_resume_->InDarkResume());

  // If the battery charge increases before a dark resume, the system should
  // resuspend.
  WriteDarkResumeState(true);
  SetBattery(61.0, false);
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(10, suspend_duration.InSeconds());
  EXPECT_TRUE(dark_resume_->InDarkResume());

  // The higher battery charge should be used as the new shutdown threshold for
  // the next dark resume.
  SetBattery(60.5, false);
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SHUT_DOWN, action);
  EXPECT_TRUE(dark_resume_->InDarkResume());
}

// Test that a new shutdown threshold is calculated when suspending from outside
// of dark resume.
TEST_F(DarkResumeTest, UserResumes) {
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10\n"
                                                    "20.0 50\n"
                                                    "50.0 100\n"
                                                    "80.0 500\n");
  Init();
  DarkResumeInterface::Action action;
  base::TimeDelta suspend_duration;
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(500, suspend_duration.InSeconds());

  SetBattery(80.0, false);
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(500, suspend_duration.InSeconds());

  SetBattery(50.0, false);
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(100, suspend_duration.InSeconds());

  SetBattery(25.0, false);
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(50, suspend_duration.InSeconds());

  SetBattery(20.0, false);
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(50, suspend_duration.InSeconds());

  SetBattery(5.0, false);
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(10, suspend_duration.InSeconds());

  SetBattery(1.0, false);
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(10, suspend_duration.InSeconds());
}

// Check that we don't shut down when on line power (regardless of the battery
// level).
TEST_F(DarkResumeTest, LinePower) {
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");
  Init();
  DarkResumeInterface::Action action;
  base::TimeDelta suspend_duration;
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);

  WriteDarkResumeState(true);
  SetBattery(50.0, true);
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
}

TEST_F(DarkResumeTest, EnableAndDisable) {
  const base::FilePath kDeviceDir = temp_dir_.path().Append("foo");
  const base::FilePath kPowerDir = kDeviceDir.Append(DarkResume::kPowerDir);
  const base::FilePath kActivePath = kPowerDir.Append(DarkResume::kActiveFile);
  const base::FilePath kSourcePath = kPowerDir.Append(DarkResume::kSourceFile);
  ASSERT_TRUE(base::CreateDirectory(kPowerDir));

  prefs_.SetString(kDarkResumeDevicesPref, kDeviceDir.value());
  prefs_.SetString(kDarkResumeSourcesPref, kDeviceDir.value());
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");

  // Dark resume should be enabled when the object is initialized.
  Init();
  EXPECT_EQ(DarkResume::kEnabled, ReadFile(kActivePath));
  EXPECT_EQ(DarkResume::kEnabled, ReadFile(kSourcePath));

  // Dark resume should be disabled when the object is destroyed.
  dark_resume_.reset();
  EXPECT_EQ(DarkResume::kDisabled, ReadFile(kActivePath));
  EXPECT_EQ(DarkResume::kDisabled, ReadFile(kSourcePath));

  // Set the "disable" pref and check that the files aren't set to the enabled
  // state after initializing a new object.
  prefs_.SetInt64(kDisableDarkResumePref, 1);
  dark_resume_.reset(new DarkResume);
  Init();
  EXPECT_EQ(DarkResume::kDisabled, ReadFile(kActivePath));
  EXPECT_EQ(DarkResume::kDisabled, ReadFile(kSourcePath));
  DarkResumeInterface::Action action;
  base::TimeDelta suspend_duration;
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(0, suspend_duration.InSeconds());

  // When the "disable" pref is set to 0, dark resume should be enabled.
  prefs_.SetInt64(kDisableDarkResumePref, 0);
  dark_resume_.reset(new DarkResume);
  Init();
  EXPECT_EQ(DarkResume::kEnabled, ReadFile(kActivePath));
  EXPECT_EQ(DarkResume::kEnabled, ReadFile(kSourcePath));
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(10, suspend_duration.InSeconds());

  // An empty suspend durations pref should result in dark resume being
  // disabled.
  prefs_.SetString(kDarkResumeSuspendDurationsPref, std::string());
  dark_resume_.reset(new DarkResume);
  Init();
  EXPECT_EQ(DarkResume::kDisabled, ReadFile(kActivePath));
  EXPECT_EQ(DarkResume::kDisabled, ReadFile(kSourcePath));
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(0, suspend_duration.InSeconds());
}

TEST_F(DarkResumeTest, PowerStatusRefreshFails) {
  prefs_.SetString(kDarkResumeSuspendDurationsPref, "0.0 10");
  Init();

  // If refreshing the power status fails, the system should suspend
  // indefinitely.
  SetBattery(80.0, false);
  power_supply_.set_refresh_result(false);
  DarkResumeInterface::Action action;
  base::TimeDelta suspend_duration;
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(0, suspend_duration.InSeconds());

  // Now let the system suspend.
  power_supply_.set_refresh_result(true);
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(10, suspend_duration.InSeconds());

  // If the refresh fails while in dark resume, the system should again suspend
  // indefinitely.
  WriteDarkResumeState(true);
  power_supply_.set_refresh_result(false);
  dark_resume_->PrepareForSuspendAttempt(&action, &suspend_duration);
  EXPECT_EQ(DarkResumeInterface::SUSPEND, action);
  EXPECT_EQ(0, suspend_duration.InSeconds());
}

}  // namespace system
}  // namespace power_manager
