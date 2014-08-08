// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/dark_resume.h"

#include <stdint.h>

#include <algorithm>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"

namespace power_manager {
namespace system {

namespace {

// Default file describing whether the system is currently in dark resume.
const char kDarkResumeStatePath[] = "/sys/power/dark_resume_state";

}  // namespace

const char DarkResume::kPowerDir[] = "power";
const char DarkResume::kActiveFile[] = "dark_resume_active";
const char DarkResume::kSourceFile[] = "dark_resume_source";
const char DarkResume::kEnabled[] = "enabled";
const char DarkResume::kDisabled[] = "disabled";

DarkResume::DarkResume()
    : power_supply_(NULL),
      prefs_(NULL),
      dark_resume_state_path_(kDarkResumeStatePath),
      battery_shutdown_threshold_(0.0) {
}

DarkResume::~DarkResume() {
  SetStates(dark_resume_sources_, false);
  SetStates(dark_resume_devices_, false);
}

void DarkResume::Init(PowerSupplyInterface* power_supply,
                      PrefsInterface* prefs) {
  power_supply_ = power_supply;
  prefs_ = prefs;

  bool disable = false;
  enabled_ = (!prefs_->GetBool(kDisableDarkResumePref, &disable) || !disable) &&
      ReadSuspendDurationsPref();
  VLOG(1) << "Dark resume user space " << (enabled_ ? "enabled" : "disabled");
  GetFiles(&dark_resume_sources_, kDarkResumeSourcesPref, kSourceFile);
  GetFiles(&dark_resume_devices_, kDarkResumeDevicesPref, kActiveFile);
  SetStates(dark_resume_sources_, enabled_);
  SetStates(dark_resume_devices_, enabled_);
}

void DarkResume::PrepareForSuspendAttempt(Action* action,
                                          base::TimeDelta* suspend_duration) {
  DCHECK(action);
  DCHECK(suspend_duration);

  if (!enabled_ || !power_supply_->RefreshImmediately()) {
    *action = SUSPEND;
    *suspend_duration = base::TimeDelta();
    return;
  }

  const PowerStatus status = power_supply_->GetPowerStatus();
  const double battery = status.battery_percentage;
  const bool line_power = status.line_power_on;
  const bool in_dark_resume = InDarkResume();
  LOG(INFO) << (in_dark_resume ? "In" : "Not in") << " dark resume with "
            << "battery at " << battery << "% and line power "
            << (line_power ? "on" : "off");

  // If suspending from the non-dark-resume state, or if the battery level has
  // actually increased since the previous suspend attempt, update the shutdown
  // threshold.
  if (!in_dark_resume || battery > battery_shutdown_threshold_) {
    battery_shutdown_threshold_ = battery;
    LOG(INFO) << "Updated shutdown threshold to " << battery << "%";
  }

  if (battery < battery_shutdown_threshold_ && !line_power) {
    *action = SHUT_DOWN;
    return;
  }

  // Determine how long the system should suspend.
  *action = SUSPEND;
  SuspendMap::iterator suspend_it = suspend_durations_.upper_bound(battery);
  if (suspend_it != suspend_durations_.begin())
    suspend_it--;
  *suspend_duration = suspend_it->second;
}

bool DarkResume::InDarkResume() {
  if (!enabled_)
    return false;

  std::string buf;
  if (!base::ReadFileToString(dark_resume_state_path_, &buf)) {
    PLOG(ERROR) << "Unable to read " << dark_resume_state_path_.value();
    return false;
  }
  base::TrimWhitespaceASCII(buf, base::TRIM_TRAILING, &buf);
  uint64_t value = 0;
  return base::StringToUint64(buf, &value) && value;
}

bool DarkResume::ReadSuspendDurationsPref() {
  suspend_durations_.clear();

  std::string data;
  if (!prefs_->GetString(kDarkResumeSuspendDurationsPref, &data))
    return false;

  base::StringPairs pairs;
  base::TrimWhitespaceASCII(data, base::TRIM_TRAILING, &data);
  if (!base::SplitStringIntoKeyValuePairs(data, ' ', '\n', &pairs)) {
    LOG(ERROR) << "Unable to parse " << kDarkResumeSuspendDurationsPref;
    return false;
  }
  for (size_t i = 0; i < pairs.size(); ++i) {
    double battery_level = 0.0;
    int suspend_duration = 0;
    if (!base::StringToDouble(pairs[i].first, &battery_level) ||
        !base::StringToInt(pairs[i].second,  &suspend_duration)) {
      LOG(ERROR) << "Unable to parse values on line " << i << " of "
                 << kDarkResumeSuspendDurationsPref;
      return false;
    }

    if (suspend_duration % base::TimeDelta::FromDays(1).InSeconds() == 0) {
      LOG(ERROR) << "Suspend duration in " << kDarkResumeSuspendDurationsPref
                 << " cannot be multiple of 86400";
      return false;
    }

    suspend_durations_[battery_level] =
        base::TimeDelta::FromSeconds(suspend_duration);
  }
  return !suspend_durations_.empty();
}

void DarkResume::GetFiles(std::vector<base::FilePath>* files,
                          const std::string& pref_name,
                          const std::string& base_file) {
  files->clear();

  std::string data;
  if (!prefs_->GetString(pref_name, &data))
    return;

  std::vector<std::string> lines;
  base::SplitString(data, '\n', &lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    base::FilePath path = base::FilePath(lines[i]);
    path = path.AppendASCII(kPowerDir);
    path = path.AppendASCII(base_file.c_str());
    files->push_back(path);
  }
}

void DarkResume::SetStates(const std::vector<base::FilePath>& files,
                           bool enabled) {
  const char* state = enabled ? kEnabled : kDisabled;
  for (std::vector<base::FilePath>::const_iterator iter = files.begin();
       iter != files.end(); ++iter) {
    if (base::WriteFile(*iter, state, strlen(state)) !=
        static_cast<int>(strlen(state))) {
      PLOG(ERROR) << "Failed writing \"" << state << "\" to " << iter->value();
    }
  }
}

}  // namespace system
}  // namespace power_manager
