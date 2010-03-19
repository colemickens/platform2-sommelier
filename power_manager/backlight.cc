// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/backlight.h"
#include <dirent.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string>
#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_util.h"

namespace power_manager {

bool Backlight::Init() {
  std::string base_path = "/sys/class/backlight/";
  DIR* dir = opendir(base_path.c_str());

#if !defined(OS_LINUX) && !defined(OS_MACOSX) && !defined(OS_FREEBSD)
  #error Port warning: depending on the definition of struct dirent, \
         additional space for pathname may be needed
#endif

  if (dir) {
    struct dirent dent_buf;
    struct dirent* dent;
    while (readdir_r(dir, &dent_buf, &dent) == 0 && dent) {
      if (dent->d_name[0] && dent->d_name[0] != '.') {
        std::string dir_path = base_path + dent->d_name;
        brightness_path_ = FilePath(dir_path + "/brightness");
        actual_brightness_path_ = FilePath(dir_path + "/actual_brightness");
        max_brightness_path_ = FilePath(dir_path + "/max_brightness");
        if (!file_util::PathExists(max_brightness_path_)) {
          LOG(WARNING) << "Can't find " << max_brightness_path_.value();
        } else if (!file_util::PathExists(actual_brightness_path_)) {
          LOG(WARNING) << "Can't find " << actual_brightness_path_.value();
        } else if (!file_util::PathIsWritable(brightness_path_)) {
          LOG(WARNING) << "Can't write to " << brightness_path_.value();
        } else {
          closedir(dir);
          return true;
        }
      }
    }
    closedir(dir);
  } else {
    LOG(WARNING) << "Can't open " << base_path;
  }
  LOG(WARNING) << "Can't init backlight interface";
  return false;
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
