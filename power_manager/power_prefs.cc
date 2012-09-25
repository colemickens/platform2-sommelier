// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_prefs.h"

#include <sys/inotify.h>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

using std::string;

namespace power_manager {

static const int kFileWatchMask = IN_MODIFY | IN_CREATE | IN_DELETE;

PowerPrefs::PowerPrefs(const FilePath& pref_path)
    : pref_paths_(std::vector<FilePath>(1, pref_path)) {}

PowerPrefs::PowerPrefs(const std::vector<FilePath>& pref_paths)
    : pref_paths_(pref_paths) {}

bool PowerPrefs::StartPrefWatching(Inotify::InotifyCallback callback,
                                   gpointer data) {
  LOG(INFO) << "Starting to watch pref directory.";
  if (!notifier_.Init(callback, data))
    return false;
  CHECK(!pref_paths_.empty());
  if (notifier_.AddWatch(pref_paths_[0].value().c_str(), kFileWatchMask) < 0)
    return false;
  notifier_.Start();
  return true;
}

void PowerPrefs::GetPrefStrings(const std::string& name,
                                bool read_all,
                                std::vector<PrefReadResult>* results) {
  CHECK(results);
  for (std::vector<FilePath>::const_iterator iter = pref_paths_.begin();
       iter != pref_paths_.end();
       ++iter) {
    FilePath path = iter->Append(name);
    string buf;
    if (file_util::ReadFileToString(path, &buf)) {
      TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
      PrefReadResult result;
      result.path = path.value();
      result.value = buf;
      results->push_back(result);
      if (!read_all)
        return;
    }
  }
}

bool PowerPrefs::GetString(const char* name, string* buf) {
  if (!buf) {
    LOG(ERROR) << "Invalid return buffer!";
    return false;
  }
  std::vector<PrefReadResult> results;
  GetPrefStrings(name, false, &results);
  if (results.empty())
    return false;
  *buf = results[0].value;
  return true;
}

bool PowerPrefs::GetInt64(const char* name, int64* value) {
  std::vector<PrefReadResult> results;
  GetPrefStrings(name, true, &results);

  for (std::vector<PrefReadResult>::const_iterator iter = results.begin();
       iter != results.end();
       ++iter) {
    if (base::StringToInt64(iter->value, value))
      return true;
    else
      LOG(ERROR) << "Garbage found in " << iter->path;
  }
  return false;
}

bool PowerPrefs::SetInt64(const char* name, int64 value) {
  CHECK(!pref_paths_.empty());
  FilePath path = pref_paths_[0].Append(name);
  string buf = base::Int64ToString(value);
  int status = file_util::WriteFile(path, buf.data(), buf.size());
  LOG_IF(ERROR, -1 == status) << "Failed to write to " << path.value();
  return -1 != status;
}

bool PowerPrefs::GetDouble(const char* name, double* value) {
  std::vector<PrefReadResult> results;
  GetPrefStrings(name, true, &results);

  for (std::vector<PrefReadResult>::const_iterator iter = results.begin();
       iter != results.end();
       ++iter) {
    if (base::StringToDouble(iter->value, value))
      return true;
    else
      LOG(ERROR) << "Garbage found in " << iter->path;
  }
  return false;
}

bool PowerPrefs::SetDouble(const char* name, double value) {
  CHECK(!pref_paths_.empty());
  FilePath path = pref_paths_[0].Append(name);
  string buf = base::DoubleToString(value);
  int status = file_util::WriteFile(path, buf.data(), buf.size());
  LOG_IF(ERROR, -1 == status) << "Failed to write to " << path.value();
  return -1 != status;
}

bool PowerPrefs::GetBool(const char* name, bool* value) {
  int64 int_value = 0;
  if (!GetInt64(name, &int_value))
    return false;
  *value = static_cast<bool>(int_value);
  return true;
}

}  // namespace power_manager
