// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_prefs.h"

#include <string>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

using std::string;

namespace power_manager {

PowerPrefs::PowerPrefs(const FilePath& pref_path, const FilePath& default_path)
    : pref_path_(pref_path),
      default_path_(default_path) {}

bool PowerPrefs::ReadSetting(const char* setting_name, int64* val) {
  FilePath path = pref_path_.Append(setting_name);
  string buf;
  if (file_util::ReadFileToString(path, &buf)) {
    TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
    if (base::StringToInt64(buf, val))
      return true;
    else
      LOG(ERROR) << "Garbage found in " << path.value();
  }
  path = default_path_.Append(setting_name);
  buf.clear();
  if (file_util::ReadFileToString(path, &buf)) {
    TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
    if (base::StringToInt64(buf, val))
      return true;
    else
      LOG(ERROR) << "Garbage found in " << path.value();
  }
  LOG(INFO) << "Could not read " << path.value();
  return false;
}

bool PowerPrefs::WriteSetting(const char* setting_name, int64 val) {
  int status;
  FilePath path = pref_path_.Append(setting_name);
  string buf = base::Int64ToString(val);
  status = file_util::WriteFile(path, buf.data(), buf.size());
  LOG_IF(ERROR, -1 == status) << "Failed to write to " << path.value();
  return -1 != status;
}

}  // namespace power_manager
