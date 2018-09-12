// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/legacy_dark_resume.h"

#include <stdint.h>

#include <algorithm>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <components/timers/alarm_timer_chromeos.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

// Default file describing whether the system is currently in dark resume.
const char kDarkResumeStatePath[] = "/sys/power/dark_resume_state";

// In kernel 3.14 and later, we switch over to wakeup_type instead of
// dark_resume_state.
const char kWakeupTypePath[] = "/sys/power/wakeup_type";

// If we go from a dark resume directly to a full resume several devices will be
// left in an awkward state.  This won't be a problem once selective resume is
// ready but until then we need to fake it by using the pm_test mechanism to
// make sure all the drivers go through the proper resume path.
// TODO(chirantan): Remove these once selective resume is ready.
const char kPMTestPath[] = "/sys/power/pm_test";
const char kPMTestDevices[] = "devices";
const char kPMTestNone[] = "none";
const char kPowerStatePath[] = "/sys/power/state";
const char kPowerStateMem[] = "mem";

// TODO(chirantan): We use the existence of this file to determine if the system
// can safely exit dark resume.  However, this file will go away once selective
// resume lands, at which point we are probably going to have to use a pref file
// instead.
const char kPMTestDelayPath[] = "/sys/power/pm_test_delay";

}  // namespace

const char LegacyDarkResume::kPowerDir[] = "power";
const char LegacyDarkResume::kActiveFile[] = "dark_resume_active";
const char LegacyDarkResume::kSourceFile[] = "dark_resume_source";
const char LegacyDarkResume::kEnabled[] = "enabled";
const char LegacyDarkResume::kDisabled[] = "disabled";
// For kernel 3.14 and later
const char LegacyDarkResume::kWakeupTypeFile[] = "wakeup_type";
const char LegacyDarkResume::kAutomatic[] = "automatic";
const char LegacyDarkResume::kUnknown[] = "unknown";

bool LegacyDarkResume::ShouldUse(PrefsInterface* prefs) {
  std::string data;
  return prefs->GetString(kDarkResumeSuspendDurationsPref, &data) &&
         !data.empty();
}

LegacyDarkResume::LegacyDarkResume()
    : in_dark_resume_(false),
      enabled_(false),
      using_wakeup_type_(false),
      can_safely_exit_dark_resume_(false),
      power_supply_(NULL),
      prefs_(NULL),
      legacy_state_path_(kDarkResumeStatePath),
      wakeup_state_path_(kWakeupTypePath),
      pm_test_path_(kPMTestPath),
      pm_test_delay_path_(kPMTestDelayPath),
      power_state_path_(kPowerStatePath),
      battery_shutdown_threshold_(0.0),
      next_action_(Action::SUSPEND) {}

LegacyDarkResume::~LegacyDarkResume() {
  if (enabled_) {
    SetStates(dark_resume_sources_, using_wakeup_type_ ? kUnknown : kDisabled);
    SetStates(dark_resume_devices_, kDisabled);
  }
}

void LegacyDarkResume::Init(PowerSupplyInterface* power_supply,
                            PrefsInterface* prefs) {
  power_supply_ = power_supply;
  prefs_ = prefs;

  auto timer = std::make_unique<timers::SimpleAlarmTimer>();
  if (timer->CanWakeFromSuspend())
    timer_ = std::move(timer);

  enabled_ = ReadSuspendDurationsPref();
  VLOG(1) << " Legacy dark resume user space "
          << (enabled_ ? "enabled" : "disabled");

  std::string source_file;
  std::string source_state;
  state_path_ = wakeup_state_path_;
  if (base::PathExists(state_path_)) {
    using_wakeup_type_ = true;
    source_file = kWakeupTypeFile;
    source_state = enabled_ ? kAutomatic : kUnknown;
  } else if (base::PathExists(legacy_state_path_)) {
    state_path_ = legacy_state_path_;
    using_wakeup_type_ = false;
    source_file = kSourceFile;
    source_state = enabled_ ? kEnabled : kDisabled;
  } else {
    enabled_ = false;
    LOG(WARNING) << "Dark resume state path not found";
  }
  if (enabled_) {
    GetFiles(&dark_resume_sources_, kDarkResumeSourcesPref, source_file);
    GetFiles(&dark_resume_devices_, kDarkResumeDevicesPref, kActiveFile);
    SetStates(dark_resume_sources_, source_state);
    SetStates(dark_resume_devices_, (enabled_ ? kEnabled : kDisabled));
    can_safely_exit_dark_resume_ = base::PathExists(pm_test_delay_path_);
  }
}

void LegacyDarkResume::PrepareForSuspendRequest() {
  if (timer_ && enabled_ && !battery_check_suspend_durations_.empty())
    ScheduleBatteryCheck();
}

void LegacyDarkResume::UndoPrepareForSuspendRequest() {
  if (timer_)
    timer_->Stop();

  in_dark_resume_ = false;
}

void LegacyDarkResume::GetActionForSuspendAttempt(
    Action* action, base::TimeDelta* suspend_duration) {
  DCHECK(action);
  DCHECK(suspend_duration);

  if (!enabled_ || battery_check_suspend_durations_.empty() ||
      !power_supply_->RefreshImmediately()) {
    *action = Action::SUSPEND;
    *suspend_duration = base::TimeDelta();
    return;
  }

  if (timer_) {
    *suspend_duration = base::TimeDelta();
  } else {
    UpdateNextAction();
    *suspend_duration = GetNextSuspendDuration();
  }

  *action = next_action_;
}

void LegacyDarkResume::ScheduleBatteryCheck() {
  if (!power_supply_->RefreshImmediately())
    return;

  UpdateNextAction();

  timer_->Start(FROM_HERE, GetNextSuspendDuration(),
                base::Bind(&LegacyDarkResume::ScheduleBatteryCheck,
                           base::Unretained(this)));
}

base::TimeDelta LegacyDarkResume::GetNextSuspendDuration() {
  if (battery_check_suspend_durations_.empty())
    return base::TimeDelta();
  const double battery = power_supply_->GetPowerStatus().battery_percentage;
  SuspendMap::iterator suspend_it =
      battery_check_suspend_durations_.upper_bound(battery);
  if (suspend_it != battery_check_suspend_durations_.begin())
    suspend_it--;
  return suspend_it->second;
}

void LegacyDarkResume::UpdateNextAction() {
  const PowerStatus status = power_supply_->GetPowerStatus();
  const double battery = status.battery_percentage;
  const bool line_power = status.line_power_on;
  const bool in_dark_resume = InDarkResume();
  LOG(INFO) << (in_dark_resume ? "In" : "Not in") << " dark resume with "
            << "battery at " << battery << "% and line power "
            << (line_power ? "on" : "off");

  // If suspending from the non-dark-resume state, or if the battery level has
  // actually increased since the previous suspend attempt, update the shutdown
  // threshold if battery_check_suspend_durations_ are present. If
  // battery_check_suspend_durations is empty, battery_shutdown_threshold_
  // doesn't matter.
  if (!battery_check_suspend_durations_.empty() &&
      (!in_dark_resume || battery > battery_shutdown_threshold_)) {
    battery_shutdown_threshold_ = battery;
    LOG(INFO) << "Updated shutdown threshold to " << battery << "%";
  }

  next_action_ = Action::SUSPEND;
  if (!battery_check_suspend_durations_.empty() &&
      battery < battery_shutdown_threshold_ && !line_power)
    next_action_ = Action::SHUT_DOWN;
}

void LegacyDarkResume::HandleSuccessfulResume() {
  if (!enabled_) {
    in_dark_resume_ = false;
    return;
  }

  std::string buf;
  if (!base::ReadFileToString(state_path_, &buf)) {
    PLOG(ERROR) << "Unable to read " << state_path_.value();
    in_dark_resume_ = false;
    return;
  }

  base::TrimWhitespaceASCII(buf, base::TRIM_TRAILING, &buf);
  if (using_wakeup_type_) {
    in_dark_resume_ = buf == kAutomatic;
  } else {
    uint64_t value = 0;
    in_dark_resume_ = base::StringToUint64(buf, &value) && value;
  }
}

bool LegacyDarkResume::InDarkResume() {
  return in_dark_resume_;
}

bool LegacyDarkResume::IsEnabled() {
  return enabled_;
}

bool LegacyDarkResume::CanSafelyExitDarkResume() {
  return can_safely_exit_dark_resume_;
}

bool LegacyDarkResume::ExitDarkResume() {
  LOG(INFO) << "Transitioning from dark resume to fully resumed.";

  // Set up the pm_test down to devices level.
  if (!util::WriteFileFully(pm_test_path_, kPMTestDevices,
                            strlen(kPMTestDevices))) {
    PLOG(ERROR) << "Unable to set up the pm_test level to properly exit dark "
                << "resume";
    return false;
  }

  // Do the pm_test suspend.
  if (!util::WriteFileFully(power_state_path_, kPowerStateMem,
                            strlen(kPowerStateMem))) {
    PLOG(ERROR) << "Error while performing a pm_test suspend to exit dark "
                << "resume";
    return false;
  }

  // Turn off pm_test so that we do a regular suspend next time.
  if (!util::WriteFileFully(pm_test_path_, kPMTestNone, strlen(kPMTestNone))) {
    PLOG(ERROR) << "Unable to restore pm_test level after attempting to exit "
                << "dark resume.";
    return false;
  }

  return true;
}

bool LegacyDarkResume::ReadSuspendDurationsPref() {
  battery_check_suspend_durations_.clear();

  std::string data;
  if (!prefs_->GetString(kDarkResumeSuspendDurationsPref, &data))
    return false;

  base::StringPairs pairs;
  base::TrimWhitespaceASCII(data, base::TRIM_TRAILING, &data);
  if (!base::SplitStringIntoKeyValuePairs(data, ' ', '\n', &pairs)) {
    LOG(ERROR) << "Unable to parse " << kDarkResumeSuspendDurationsPref;
    return false;
  }

  if (!pairs.size())
    return false;

  for (size_t i = 0; i < pairs.size(); ++i) {
    double battery_level = 0.0;
    int suspend_duration = 0;
    if (!base::StringToDouble(pairs[i].first, &battery_level) ||
        !base::StringToInt(pairs[i].second, &suspend_duration)) {
      LOG(ERROR) << "Unable to parse values on line " << i << " of "
                 << kDarkResumeSuspendDurationsPref;
      return false;
    }

    if (suspend_duration % base::TimeDelta::FromDays(1).InSeconds() == 0) {
      LOG(ERROR) << "Suspend duration in " << kDarkResumeSuspendDurationsPref
                 << " cannot be multiple of 86400";
      return false;
    }

    battery_check_suspend_durations_[battery_level] =
        base::TimeDelta::FromSeconds(suspend_duration);
  }
  return true;
}

void LegacyDarkResume::GetFiles(std::vector<base::FilePath>* files,
                                const std::string& pref_name,
                                const std::string& base_file) {
  files->clear();

  std::string data;
  if (!prefs_->GetString(pref_name, &data))
    return;

  std::vector<std::string> lines = base::SplitString(
      data, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t i = 0; i < lines.size(); ++i) {
    base::FilePath path = base::FilePath(lines[i]);
    path = path.AppendASCII(kPowerDir);
    path = path.AppendASCII(base_file.c_str());
    files->push_back(path);
  }
}

void LegacyDarkResume::SetStates(const std::vector<base::FilePath>& files,
                                 const std::string& state) {
  for (std::vector<base::FilePath>::const_iterator iter = files.begin();
       iter != files.end(); ++iter) {
    if (!util::WriteFileFully(*iter, state.c_str(), state.length())) {
      PLOG(ERROR) << "Failed writing \"" << state << "\" to " << iter->value();
    }
  }
}

}  // namespace system
}  // namespace power_manager
