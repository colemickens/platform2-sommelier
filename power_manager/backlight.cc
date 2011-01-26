// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/backlight.h"

#include <dirent.h>
#include <glib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

namespace power_manager {

// Gradually change backlight level to new brightness by breaking up the
// transition into N steps, where N = kBacklightNumSteps.
const int kBacklightNumSteps = 8;
// Time between backlight adjustment steps, in milliseconds.
const int kBacklightStepTimeMs = 30;

bool Backlight::Init() {
  FilePath base_path("/sys/class/backlight");
  DIR* dir = opendir(base_path.value().c_str());

#if !defined(OS_LINUX) && !defined(OS_MACOSX) && !defined(OS_FREEBSD)
  #error Port warning: depending on the definition of struct dirent, \
         additional space for pathname may be needed
#endif

  if (dir) {
    struct dirent dent_buf;
    struct dirent* dent;
    int64 max = 0;
    FilePath dir_path;

    // Find the backlight interface with greatest granularity (highest max).
    while (readdir_r(dir, &dent_buf, &dent) == 0 && dent) {
      if (dent->d_name[0] && dent->d_name[0] != '.') {
        FilePath check_path = base_path.Append(dent->d_name);
        int64 check_max = CheckBacklightFiles(check_path);
        if (check_max > max) {
          max = check_max;
          dir_path = check_path;
        }
      }
    }
    closedir(dir);

    // Restore path names to those of the selected interface.
    if (max > 0) {
      brightness_path_ = dir_path.Append("brightness");
      actual_brightness_path_ = dir_path.Append("actual_brightness");
      max_brightness_path_ = dir_path.Append("max_brightness");
      // Read brightness to initialize the target brightness value.
      int64 max_brightness;
      GetBrightness(&target_brightness_, &max_brightness);
      return true;
    }
  } else {
    LOG(WARNING) << "Can't open " << base_path.value();
  }

  LOG(WARNING) << "Can't init backlight interface";
  return false;
}

int64 Backlight::CheckBacklightFiles(const FilePath& dir_path) {
  int64 level, max_level;
  // Use members instead of locals so GetBrightness() can use them.
  brightness_path_ = dir_path.Append("brightness");
  actual_brightness_path_ = dir_path.Append("actual_brightness");
  max_brightness_path_ = dir_path.Append("max_brightness");
  if (!file_util::PathExists(max_brightness_path_)) {
    LOG(WARNING) << "Can't find " << max_brightness_path_.value();
  } else if (!file_util::PathExists(actual_brightness_path_)) {
    LOG(WARNING) << "Can't find " << actual_brightness_path_.value();
  } else if (access(brightness_path_.value().c_str(), R_OK | W_OK)) {
    LOG(WARNING) << "Can't write to " << brightness_path_.value();
  } else if (GetBrightness(&level, &max_level)) {
    return max_level;
  } // Else GetBrightness logs a warning for us.  No need to do it here.

  return 0;
}

bool Backlight::GetBrightness(int64* level, int64* max_level) {
  if (actual_brightness_path_.empty() || max_brightness_path_.empty()) {
    LOG(WARNING) << "Cannot find backlight brightness files.";
    return false;
  }
  std::string level_buf, max_level_buf;
  bool ok = file_util::ReadFileToString(actual_brightness_path_, &level_buf) &&
            file_util::ReadFileToString(max_brightness_path_, &max_level_buf);
  if (ok) {
    TrimWhitespaceASCII(level_buf, TRIM_TRAILING, &level_buf);
    TrimWhitespaceASCII(max_level_buf, TRIM_TRAILING, &max_level_buf);
    ok = base::StringToInt64(level_buf, level) &&
         base::StringToInt64(max_level_buf, max_level);
  }
  LOG_IF(WARNING, !ok) << "Can't get brightness";
  LOG(INFO) << "GetBrightness: " << *level;
  return ok;
}

bool Backlight::GetTargetBrightness(int64* level) {
  *level = target_brightness_;
  return true;
}

bool Backlight::SetBrightness(int64 target_level) {
  if (brightness_path_.empty()) {
    LOG(WARNING) << "Cannot find backlight brightness file.";
    return false;
  }
  LOG(INFO) << "SetBrightness(" << target_level << ")";
  int64 current_level, max_level;
  GetBrightness(&current_level, &max_level);
  int64 diff = target_level - current_level;
  target_brightness_ = target_level;
  for (int i = 0; i < kBacklightNumSteps; i++) {
    int64 step_level = current_level + diff * (i + 1) / kBacklightNumSteps;
    g_timeout_add(i * kBacklightStepTimeMs, SetBrightnessHardThunk,
                  CreateSetBrightnessHardArgs(this, step_level, target_level));
  }
  return true;
}

bool Backlight::GetTransitionParams(int* num_steps, int* step_time_ms)
{
  if (num_steps == NULL)
    return false;
  if (step_time_ms == NULL)
    return false;
  *num_steps = kBacklightNumSteps;
  *step_time_ms = kBacklightStepTimeMs;
  return true;
}

gboolean Backlight::SetBrightnessHard(int64 level, int64 target_level) {
  // If the target brightness of this call does not match the backlight's
  // current target brightness, it must be from an earlier backlight adjustment
  // that had a different target brightness.  In that case, it is invalidated
  // so do nothing.
  if (target_brightness_ != target_level)
    return false; // Return false so glib doesn't repeat.
  DLOG(INFO) << "Setting brightness to " << level;
  std::string buf = base::Int64ToString(level);
  bool ok = (-1 != file_util::WriteFile(brightness_path_, buf.data(),
                                        buf.size()));
  LOG_IF(WARNING, !ok) << "Can't set brightness to " << level;
  return false; // Return false so glib doesn't repeat.
}

}  // namespace power_manager
