// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/backlight.h"

#include <dirent.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_util.h"

namespace power_manager {

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
  CHECK(!brightness_path_.empty());
  CHECK(!max_brightness_path_.empty());
  std::string level_buf, max_level_buf;
  bool ok = file_util::ReadFileToString(actual_brightness_path_, &level_buf) &&
            file_util::ReadFileToString(max_brightness_path_, &max_level_buf);
  if (ok) {
    TrimWhitespaceASCII(level_buf, TRIM_TRAILING, &level_buf);
    TrimWhitespaceASCII(max_level_buf, TRIM_TRAILING, &max_level_buf);
    ok = StringToInt64(level_buf, level) &&
         StringToInt64(max_level_buf, max_level);
  }
  LOG_IF(WARNING, !ok) << "Can't get brightness";
  return ok;
}

bool Backlight::SetBrightness(int64 level) {
  std::string buf = Int64ToString(level);
  bool ok =
      -1 != file_util::WriteFile(brightness_path_, buf.data(), buf.size());
  LOG_IF(WARNING, !ok) << "Can't set brightness to " << level;
  return ok;
}

}  // namespace power_manager
