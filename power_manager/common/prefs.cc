// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/prefs.h"

#include <sys/inotify.h>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "power_manager/common/prefs_observer.h"

namespace power_manager {

namespace {

const int kFileWatchMask = IN_MODIFY | IN_CREATE | IN_DELETE;

}  // namespace

Prefs::Prefs() {}

Prefs::~Prefs() {}

bool Prefs::Init(const std::vector<FilePath>& pref_paths) {
  CHECK(!pref_paths.empty());
  pref_paths_ = pref_paths;

  if (!notifier_.Init(&Prefs::HandleFileChangedThunk, this))
    return false;
  if (notifier_.AddWatch(pref_paths_[0].AsUTF8Unsafe().c_str(),
                         kFileWatchMask) < 0) {
    return false;
  }
  notifier_.Start();
  return true;
}

void Prefs::AddObserver(PrefsObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void Prefs::RemoveObserver(PrefsObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void Prefs::HandleFileChanged(const std::string& name) {
  FOR_EACH_OBSERVER(PrefsObserver, observers_, OnPrefChanged(name));
}

void Prefs::GetPrefStrings(const std::string& name,
                           bool read_all,
                           std::vector<PrefReadResult>* results) {
  CHECK(results);
  for (std::vector<FilePath>::const_iterator iter = pref_paths_.begin();
       iter != pref_paths_.end();
       ++iter) {
    FilePath path = iter->Append(name);
    std::string buf;
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

bool Prefs::GetString(const std::string& name, std::string* buf) {
  DCHECK(buf);
  std::vector<PrefReadResult> results;
  GetPrefStrings(name, false, &results);
  if (results.empty())
    return false;
  *buf = results[0].value;
  return true;
}

bool Prefs::GetInt64(const std::string& name, int64* value) {
  DCHECK(value);
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

bool Prefs::GetDouble(const std::string& name, double* value) {
  DCHECK(value);
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

bool Prefs::GetBool(const std::string& name, bool* value) {
  int64 int_value = 0;
  if (!GetInt64(name, &int_value))
    return false;
  *value = int_value != 0;
  return true;
}

bool Prefs::SetString(const std::string& name, const std::string& value) {
  CHECK(!pref_paths_.empty());
  FilePath path = pref_paths_[0].Append(name);
  int status = file_util::WriteFile(path, value.data(), value.size());
  PLOG_IF(ERROR, status == -1) << "Failed to write to " << path.AsUTF8Unsafe();
  return status != -1;
}

bool Prefs::SetInt64(const std::string& name, int64 value) {
  CHECK(!pref_paths_.empty());
  FilePath path = pref_paths_[0].Append(name);
  std::string buf = base::Int64ToString(value);
  int status = file_util::WriteFile(path, buf.data(), buf.size());
  PLOG_IF(ERROR, status == -1) << "Failed to write to " << path.AsUTF8Unsafe();
  return status != -1;
}

bool Prefs::SetDouble(const std::string& name, double value) {
  CHECK(!pref_paths_.empty());
  FilePath path = pref_paths_[0].Append(name);
  std::string buf = base::DoubleToString(value);
  int status = file_util::WriteFile(path, buf.data(), buf.size());
  PLOG_IF(ERROR, status == -1) << "Failed to write to " << path.AsUTF8Unsafe();
  return status != -1;
}

}  // namespace power_manager
