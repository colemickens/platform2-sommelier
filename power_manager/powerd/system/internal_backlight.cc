// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/internal_backlight.h"

#include <glib.h>

#include <cmath>
#include <string>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

// When animating a brightness level transition, amount of time in milliseconds
// to wait between each update.
const guint kTransitionIntervalMs = 30;

}  // namespace

const char InternalBacklight::kBrightnessFilename[] = "brightness";
const char InternalBacklight::kMaxBrightnessFilename[] = "max_brightness";
const char InternalBacklight::kActualBrightnessFilename[] = "actual_brightness";
const char InternalBacklight::kResumeBrightnessFilename[] = "resume_brightness";

InternalBacklight::InternalBacklight()
    : max_brightness_level_(0),
      current_brightness_level_(0),
      transition_timeout_id_(0),
      transition_start_level_(0),
      transition_end_level_(0) {
}

InternalBacklight::~InternalBacklight() {
  CancelTransition();
}

bool InternalBacklight::Init(const FilePath& base_path,
                             const FilePath::StringType& pattern) {
  FilePath dir_path;
  file_util::FileEnumerator dir_enumerator(
      base_path, false, file_util::FileEnumerator::DIRECTORIES, pattern);

  // Find the backlight interface with greatest granularity (highest max).
  for (FilePath check_path = dir_enumerator.Next(); !check_path.empty();
       check_path = dir_enumerator.Next()) {
    FilePath check_name = check_path.BaseName();
    std::string str_check_name = check_name.value();
    if (str_check_name[0] == '.')
      continue;

    int64 max = CheckBacklightFiles(check_path);
    if (max <= max_brightness_level_)
      continue;

    max_brightness_level_ = max;
    GetBacklightFilePaths(check_path,
                          &actual_brightness_path_,
                          &brightness_path_,
                          &max_brightness_path_,
                          &resume_brightness_path_);

    // Technically all screen backlights should implement actual_brightness,
    // but we'll handle ones that don't.  This allows us to work with keyboard
    // backlights too.
    if (!file_util::PathExists(actual_brightness_path_))
      actual_brightness_path_ = brightness_path_;
  }

  if (max_brightness_level_ <= 0) {
    LOG(ERROR) << "Can't init backlight interface";
    return false;
  }

  ReadBrightnessLevelFromFile(actual_brightness_path_,
                              &current_brightness_level_);
  return true;
}

gboolean InternalBacklight::TriggerTransitionTimeoutForTesting() {
  return HandleTransitionTimeout();
}

bool InternalBacklight::GetMaxBrightnessLevel(int64* max_level) {
  DCHECK(max_level);
  *max_level = max_brightness_level_;
  return true;
}

bool InternalBacklight::GetCurrentBrightnessLevel(int64* current_level) {
  DCHECK(current_level);
  *current_level = current_brightness_level_;
  return true;
}

bool InternalBacklight::SetResumeBrightnessLevel(int64 level) {
  if (resume_brightness_path_.empty()) {
    LOG(ERROR) << "Cannot find backlight resume brightness file.";
    return false;
  }

  return WriteBrightnessLevelToFile(resume_brightness_path_, level);
}

bool InternalBacklight::SetBrightnessLevel(int64 level,
                                           base::TimeDelta interval) {
  if (brightness_path_.empty()) {
    LOG(ERROR) << "Cannot find backlight brightness file.";
    return false;
  }

  CancelTransition();

  if (level == current_brightness_level_)
    return true;

  if (interval.InMilliseconds() <= kTransitionIntervalMs) {
    if (!WriteBrightnessLevelToFile(brightness_path_, level))
      return false;
    current_brightness_level_ = level;
    return true;
  }

  transition_start_time_ = GetCurrentTime();
  transition_end_time_ = transition_start_time_ + interval;
  transition_start_level_ = current_brightness_level_;
  transition_end_level_ = level;
  transition_timeout_id_ =
      g_timeout_add(kTransitionIntervalMs, HandleTransitionTimeoutThunk, this);
  return true;
}

// static
void InternalBacklight::GetBacklightFilePaths(const FilePath& dir_path,
                                              FilePath* actual_brightness_path,
                                              FilePath* brightness_path,
                                              FilePath* max_brightness_path,
                                              FilePath* resume_brightness_path) {
  if (actual_brightness_path)
    *actual_brightness_path = dir_path.Append(kActualBrightnessFilename);
  if (brightness_path)
    *brightness_path = dir_path.Append(kBrightnessFilename);
  if (max_brightness_path)
    *max_brightness_path = dir_path.Append(kMaxBrightnessFilename);
  if (resume_brightness_path)
    *resume_brightness_path = dir_path.Append(kResumeBrightnessFilename);
}

// static
int64 InternalBacklight::CheckBacklightFiles(const FilePath& dir_path) {
  FilePath actual_brightness_path, brightness_path, max_brightness_path,
           resume_brightness_path;
  GetBacklightFilePaths(dir_path,
                        &actual_brightness_path,
                        &brightness_path,
                        &max_brightness_path,
                        &resume_brightness_path);

  if (!file_util::PathExists(max_brightness_path)) {
    LOG(WARNING) << "Can't find " << max_brightness_path.value();
    return 0;
  } else if (access(brightness_path.value().c_str(), R_OK | W_OK)) {
    LOG(WARNING) << "Can't write to " << brightness_path.value();
    return 0;
  }

  int64 max_level = 0;
  if (!ReadBrightnessLevelFromFile(max_brightness_path, &max_level))
    return 0;
  return max_level;
}

// static
bool InternalBacklight::ReadBrightnessLevelFromFile(const FilePath& path,
                                                    int64* level) {
  DCHECK(level);

  std::string level_str;
  if (!file_util::ReadFileToString(path, &level_str)) {
    LOG(ERROR) << "Unable to read brightness from " << path.value();
    return false;
  }

  TrimWhitespaceASCII(level_str, TRIM_TRAILING, &level_str);
  if (!base::StringToInt64(level_str, level)) {
    LOG(ERROR) << "Unable to parse brightness \"" << level_str << "\" from "
               << path.value();
    return false;
  }

  return true;
}

// static
bool InternalBacklight::WriteBrightnessLevelToFile(const FilePath& path,
                                                   int64 level) {
  std::string buf = base::Int64ToString(level);
  if (file_util::WriteFile(path, buf.data(), buf.size()) == -1) {
    LOG(ERROR) << "Unable to write brightness \"" << buf << "\" to "
               << path.value();
    return false;
  }
  return true;
}

base::TimeTicks InternalBacklight::GetCurrentTime() const {
  return current_time_for_testing_.is_null() ?
         base::TimeTicks::Now() :
         current_time_for_testing_;
}

gboolean InternalBacklight::HandleTransitionTimeout() {
  base::TimeTicks now = GetCurrentTime();
  int64 new_level = 0;
  gboolean should_repeat_timeout = TRUE;

  if (now >= transition_end_time_) {
    new_level = transition_end_level_;
    transition_timeout_id_ = 0;
    should_repeat_timeout = FALSE;
  } else {
    double transition_fraction =
        (now - transition_start_time_).InMillisecondsF() /
        (transition_end_time_ - transition_start_time_).InMillisecondsF();
    int64 intermediate_amount = lround(
        transition_fraction *
        (transition_end_level_ - transition_start_level_));
    new_level = transition_start_level_ + intermediate_amount;
  }

  if (WriteBrightnessLevelToFile(brightness_path_, new_level))
    current_brightness_level_ = new_level;
  return should_repeat_timeout;
}

void InternalBacklight::CancelTransition() {
  util::RemoveTimeout(&transition_timeout_id_);
  transition_start_time_ = base::TimeTicks();
  transition_end_time_ = base::TimeTicks();
  transition_start_level_ = current_brightness_level_;
  transition_end_level_ = current_brightness_level_;
}

}  // namespace system
}  // namespace power_manager
