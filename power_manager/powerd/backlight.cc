// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/backlight.h"

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

Backlight::Backlight() : max_brightness_level_(0) {}

bool Backlight::Init(const FilePath& base_path,
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
                          &max_brightness_path_);

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
  return true;
}

bool Backlight::GetMaxBrightnessLevel(int64* max_level) {
  DCHECK(max_level);
  *max_level = max_brightness_level_;
  return true;
}

bool Backlight::GetCurrentBrightnessLevel(int64* current_level) {
  return ReadBrightnessLevelFromFile(actual_brightness_path_, current_level);
}

bool Backlight::SetBrightnessLevel(int64 level) {
  if (brightness_path_.empty()) {
    LOG(ERROR) << "Cannot find backlight brightness file.";
    return false;
  }

  std::string buf = base::Int64ToString(level);
  if (file_util::WriteFile(brightness_path_, buf.data(), buf.size()) == -1) {
    LOG(ERROR) << "Unable to write brightness \"" << buf << "\" to "
               << brightness_path_.value();
    return false;
  }

  return true;
}

// static
void Backlight::GetBacklightFilePaths(const FilePath& dir_path,
                                      FilePath* actual_brightness_path,
                                      FilePath* brightness_path,
                                      FilePath* max_brightness_path) {
  if (actual_brightness_path)
    *actual_brightness_path = dir_path.Append("actual_brightness");
  if (brightness_path)
    *brightness_path = dir_path.Append("brightness");
  if (max_brightness_path)
    *max_brightness_path = dir_path.Append("max_brightness");
}

// static
int64 Backlight::CheckBacklightFiles(const FilePath& dir_path) {
  FilePath actual_brightness_path, brightness_path, max_brightness_path;
  GetBacklightFilePaths(dir_path,
                        &actual_brightness_path,
                        &brightness_path,
                        &max_brightness_path);

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
bool Backlight::ReadBrightnessLevelFromFile(const FilePath& path,
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

}  // namespace power_manager
