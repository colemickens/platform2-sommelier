// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_prefs.h"

#include <string>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_util.h"

using std::string;

namespace power_manager {

PowerPrefs::PowerPrefs(const FilePath& base_path)
    : base_path_(base_path) {}

bool PowerPrefs::ReadSetting(const char* setting_name, int64* val) {
  FilePath path = base_path_.Append(setting_name);
  string buf;
  if (!file_util::ReadFileToString(path, &buf))
    return false;
  TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
  return StringToInt64(buf, val);
}

bool PowerPrefs::WriteSetting(const char* setting_name, int64 val) {
  FilePath path = base_path_.Append(setting_name);
  string buf = Int64ToString(val);
  return -1 != file_util::WriteFile(path, buf.data(), buf.size());
}

}  // namespace power_manager
