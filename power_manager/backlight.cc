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

Backlight::Backlight() {}

bool Backlight::Init(const FilePath& base_path,
                     const FilePath::StringType& pattern) {
  int64 max = 0;
  FilePath dir_path;
  file_util::FileEnumerator dir_enumerator(base_path, false,
      file_util::FileEnumerator::DIRECTORIES, pattern);

  // Find the backlight interface with greatest granularity (highest max).
  for (FilePath check_path = dir_enumerator.Next(); !check_path.empty();
       check_path = dir_enumerator.Next()) {
    FilePath check_name = check_path.BaseName();
    std::string str_check_name = check_name.value();

    if (str_check_name[0] != '.') {
      int64 check_max = CheckBacklightFiles(check_path);
      if (check_max > max) {
        max = check_max;
        dir_path = check_path;
      }
    }
  }

  // Restore path names to those of the selected interface.
  if (max > 0) {
    CheckBacklightFiles(dir_path);
    return true;
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
    return 0;
  } else if (access(brightness_path_.value().c_str(), R_OK | W_OK)) {
    LOG(WARNING) << "Can't write to " << brightness_path_.value();
    return 0;
  }

  // Technically all screen backlights should implement actual_brightness, but
  // we'll handle ones that don't.  This allows us to work with keyboard
  // backlights too.
  if (!file_util::PathExists(actual_brightness_path_))
    actual_brightness_path_ = brightness_path_;

  if (GetBrightness(&level, &max_level)) {
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

bool Backlight::SetBrightness(int64 level) {
  if (brightness_path_.empty()) {
    LOG(WARNING) << "Cannot find backlight brightness file.";
    return false;
  }
  std::string buf = base::Int64ToString(level);
  bool ok = (-1 != file_util::WriteFile(brightness_path_, buf.data(),
                                        buf.size()));
  LOG_IF(WARNING, !ok) << "Can't write [" << level << "] to "
                       << brightness_path_.value().c_str();
  return ok;
}

}  // namespace power_manager
