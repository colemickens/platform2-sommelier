// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/dark_resume_policy.h"

#include <algorithm>

#include <base/file_util.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/time/time.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace policy {

namespace {

const char kDarkResumeStatePath[] = "/sys/power/dark_resume_state";

// Within a device directory there is a directory named power/ which contain two
// files for every device, dark_resume_active and dark_resume_source. Given the
// path to the device, we can get to these files to enable dark resume
// functionality for the device by appending |kPowerDir| then |kDarkResume...|
// to the path.
const char kDarkResumeActive[] = "dark_resume_active";
const char kDarkResumeSource[] = "dark_resume_source";
const char kPowerDir[] = "power/";

// Strings to write to sysfs files to enable/disable dark resume functionality
// on the kernel level.
const char kEnabled[] = "enabled";
const char kDisabled[] = "disabled";

}  // namespace

DarkResumePolicy::DarkResumePolicy()
    : power_supply_(NULL),
      prefs_(NULL),
      battery_shutdown_threshold_(0.0),
      battery_suspend_level_(0.0),
      thresholds_set_(false) {
}

DarkResumePolicy::~DarkResumePolicy() {
  SetStates(dark_resume_sources_, kDisabled);
  SetStates(dark_resume_devices_, kDisabled);
}

void DarkResumePolicy::Init(system::PowerSupply* power_supply,
                            PrefsInterface* prefs) {
  power_supply_ = power_supply;
  prefs_ = prefs;

  bool disable = false;
  enabled_ = (!prefs_->GetBool(kDisableDarkResumePref, &disable) || !disable) &&
              ReadSuspendDurationsPref() &&
              ReadBatteryMarginsPref();
  LOG(INFO) << "Dark resume user space " << (enabled_ ? "enabled" : "disabled");
  GetFiles(&dark_resume_sources_, kDarkResumeSourcesPref, kDarkResumeSource);
  GetFiles(&dark_resume_devices_, kDarkResumeDevicesPref, kDarkResumeActive);
  SetStates(dark_resume_sources_, (enabled_ ? kEnabled : kDisabled));
  SetStates(dark_resume_devices_, (enabled_ ? kEnabled : kDisabled));
}

DarkResumePolicy::Action DarkResumePolicy::GetAction() {
  if (!enabled_)
    return SUSPEND_INDEFINITELY;

  power_supply_->RefreshImmediately();
  power_status_ = power_supply_->power_status();
  LOG(INFO) << "Current battery is " << power_status_.battery_percentage
            << "% with line power "
            << (power_status_.line_power_on ? "on" : "off");
  if (!thresholds_set_)
    SetThresholds();
  if (power_status_.battery_percentage < battery_shutdown_threshold_ &&
      !power_status_.line_power_on)
    return SHUT_DOWN;

  if (power_status_.battery_percentage > battery_suspend_level_)
    SetThresholds();

  return SUSPEND_FOR_DURATION;
}

base::TimeDelta DarkResumePolicy::GetSuspendDuration() {
  if (!enabled_)
    return base::TimeDelta();

  double battery = power_status_.battery_percentage;
  SuspendMap::iterator upper = suspend_durations_.upper_bound(battery);

  if (upper != suspend_durations_.begin())
    upper--;

  return upper->second;
}

bool DarkResumePolicy::CurrentlyInDarkResume() {
  std::string buf;
  if (!base::ReadFileToString(base::FilePath(kDarkResumeStatePath), &buf)) {
    PLOG(ERROR) << "Unable to read " << kDarkResumeStatePath;
    return false;
  }
  TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
  uint64 value = 0;
  return base::StringToUint64(buf, &value) && value;
}

void DarkResumePolicy::HandleResume() {
  battery_suspend_level_ = 0.0;
  battery_shutdown_threshold_ = 0.0;
  thresholds_set_ = false;
}

bool DarkResumePolicy::ExtractLines(const std::string& pref_name,
                                    std::vector<std::string>* lines) {
  std::string input_str;
  if (prefs_->GetString(pref_name, &input_str)) {
    base::SplitString(input_str, '\n', lines);
    return true;
  }
  return false;
}

bool DarkResumePolicy::ReadSuspendDurationsPref() {
  suspend_durations_.clear();
  std::vector<std::string> lines;
  if (!ExtractLines(kDarkResumeSuspendDurationsPref, &lines))
    return false;

  for (std::vector<std::string>::iterator iter = lines.begin();
       iter != lines.end(); ++iter) {
    std::vector<std::string> segments;
    base::SplitString(*iter, ' ', &segments);
    if (segments.size() != 2) {
      LOG(ERROR) << "Skipping line in dark resume suspend durations file:"
                 << *iter;
      return false;
    }

    double battery_level;
    int suspend_duration;
    if (!base::StringToDouble(segments[0], &battery_level) ||
        !base::StringToInt(segments[1],  &suspend_duration)) {
      LOG(ERROR) << "Failure in parse string: " << *iter << " -> ("
                 << segments[0] << ", "
                 << segments[1] << ")";
      return false;
    }

    if (suspend_duration % base::TimeDelta::FromDays(1).InSeconds() == 0) {
      LOG(ERROR) << "Cannot have suspend duration of 0 or a multiple of 86400"
                 << " (number of seconds in a day).";
      return false;
    }

    suspend_durations_[battery_level] =
        base::TimeDelta::FromSeconds(suspend_duration);
  }
  return !suspend_durations_.empty();
}

bool DarkResumePolicy::ReadBatteryMarginsPref() {
  battery_margins_.clear();
  std::vector<std::string> lines;
  if (!ExtractLines(kDarkResumeBatteryMarginsPref, &lines))
    return false;

  for (std::vector<std::string>::iterator iter = lines.begin();
       iter != lines.end(); ++iter) {
    std::vector<std::string> segments;
    base::SplitString(*iter, ' ', &segments);
    if (segments.size() != 2) {
      LOG(ERROR) << "Skipping line in dark resume battery margins file:"
                 << *iter;
      return false;
    }

    double battery_level;
    double margin;
    if (!base::StringToDouble(segments[0], &battery_level) ||
        !base::StringToDouble(segments[1], &margin)) {
      LOG(ERROR) << "Failure in parse string: " << *iter << " -> ("
                 << segments[0] << ", "
                 << segments[1] << ")";
      return false;
    }

    battery_margins_[battery_level] = margin;
  }
  return !battery_margins_.empty();
}

void DarkResumePolicy::GetFiles(std::vector<base::FilePath>* files,
                                const std::string& pref_name,
                                const std::string& base_file) {
  files->clear();
  std::vector<std::string> lines;
  if (!ExtractLines(pref_name, &lines))
    return;

  for (std::vector<std::string>::iterator iter = lines.begin();
       iter != lines.end(); ++iter) {
    base::FilePath path = base::FilePath(*iter);
    path = path.AppendASCII(kPowerDir);
    path = path.AppendASCII(base_file.c_str());
    files->push_back(path);
  }
}

void DarkResumePolicy::SetStates(const std::vector<base::FilePath>& files,
                                 const std::string& state) {
  for (std::vector<base::FilePath>::const_iterator iter = files.begin();
       iter != files.end(); ++iter) {
    file_util::WriteFile(*iter, state.c_str(), state.length());
  }
}

void DarkResumePolicy::SetThresholds() {
  double battery = power_status_.battery_percentage;
  MarginMap::iterator margin = battery_margins_.upper_bound(battery);

  if (margin != battery_margins_.begin())
    margin--;

  battery_shutdown_threshold_ = battery - margin->second;
  battery_suspend_level_ = battery;
  thresholds_set_ = true;
  LOG(INFO) << "Current threshold is " << battery_shutdown_threshold_;
}

}  // namespace policy
}  // namespace power_manager
