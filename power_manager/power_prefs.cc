// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_prefs.h"

#include <sys/inotify.h>
#include <string>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

using std::string;

namespace power_manager {

static const int kFileWatchMask = IN_MODIFY | IN_CREATE | IN_DELETE;

PowerPrefs::PowerPrefs(const FilePath& pref_path, const FilePath& default_path)
    : pref_path_(pref_path),
      default_path_(default_path) {}

bool PowerPrefs::StartPrefWatching(Inotify::InotifyCallback callback,
                                   gpointer data) {
  LOG(INFO) << "Starting to watch pref directory.";
  if (!notifier_.Init(callback, data))
    return false;
  if (0 > notifier_.AddWatch(pref_path_.value().c_str(), kFileWatchMask))
    return false;
  notifier_.Start();
  return true;
}

bool PowerPrefs::GetString(const char* name, string* buf) {
  if (!buf) {
    LOG(ERROR) << "Invalid return buffer!";
    return false;
  }

  FilePath path = pref_path_.Append(name);
  if (file_util::ReadFileToString(path, buf)) {
    TrimWhitespaceASCII(*buf, TRIM_TRAILING, buf);
    return true;
  }
  path = default_path_.Append(name);
  buf->clear();
  if (file_util::ReadFileToString(path, buf)) {
    TrimWhitespaceASCII(*buf, TRIM_TRAILING, buf);
    return true;
  }
  return false;
}

bool PowerPrefs::GetInt64(const char* name, int64* value) {
  FilePath path = pref_path_.Append(name);
  string buf;
  if (file_util::ReadFileToString(path, &buf)) {
    TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
    if (base::StringToInt64(buf, value))
      return true;
    else
      LOG(ERROR) << "Garbage found in " << path.value();
  }
  path = default_path_.Append(name);
  buf.clear();
  if (file_util::ReadFileToString(path, &buf)) {
    TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
    if (base::StringToInt64(buf, value))
      return true;
    else
      LOG(ERROR) << "Garbage found in " << path.value();
  }
  return false;
}

bool PowerPrefs::SetInt64(const char* name, int64 value) {
  FilePath path = pref_path_.Append(name);
  string buf = base::Int64ToString(value);
  int status = file_util::WriteFile(path, buf.data(), buf.size());
  LOG_IF(ERROR, -1 == status) << "Failed to write to " << path.value();
  return -1 != status;
}

bool PowerPrefs::GetDouble(const char* name, double* value) {
  FilePath path = pref_path_.Append(name);
  string buf;
  if (file_util::ReadFileToString(path, &buf)) {
    TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
    if (base::StringToDouble(buf, value))
      return true;
    else
      LOG(ERROR) << "Garbage found in " << path.value();
  }
  path = default_path_.Append(name);
  buf.clear();
  if (file_util::ReadFileToString(path, &buf)) {
    TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
    if (base::StringToDouble(buf, value))
      return true;
    else
      LOG(ERROR) << "Garbage found in " << path.value();
  }
  return false;
}

bool PowerPrefs::SetDouble(const char* name, double value) {
  FilePath path = pref_path_.Append(name);
  string buf = base::DoubleToString(value);
  int status = file_util::WriteFile(path, buf.data(), buf.size());
  LOG_IF(ERROR, -1 == status) << "Failed to write to " << path.value();
  return -1 != status;
}

}  // namespace power_manager
