// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/prefs.h"

#include <glib.h>
#include <sys/inotify.h>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "power_manager/common/prefs_observer.h"
#include "power_manager/common/util.h"

namespace power_manager {

namespace {

const int kFileWatchMask = IN_MODIFY | IN_CREATE | IN_DELETE;

// Minimum time between batches of prefs being written to disk, in
// milliseconds.
const int kDefaultWriteIntervalMs = 1000;

}  // namespace

Prefs::TestApi::TestApi(Prefs* prefs) : prefs_(prefs) {}

Prefs::TestApi::~TestApi() {}

bool Prefs::TestApi::TriggerWriteTimeout() {
  if (!prefs_->write_prefs_timeout_id_)
    return false;

  guint timeout_id = prefs_->write_prefs_timeout_id_;
  CHECK(!prefs_->HandleWritePrefsTimeout());
  CHECK(prefs_->write_prefs_timeout_id_ == 0);
  util::RemoveTimeout(&timeout_id);
  return true;
}

Prefs::Prefs()
    : write_prefs_timeout_id_(0),
      write_interval_(
          base::TimeDelta::FromMilliseconds(kDefaultWriteIntervalMs)) {
}

Prefs::~Prefs() {
  if (write_prefs_timeout_id_) {
    util::RemoveTimeout(&write_prefs_timeout_id_);
    WritePrefs();
  }
}

bool Prefs::Init(const std::vector<base::FilePath>& pref_paths) {
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
  // Resist the temptation to erase |name| from |prefs_to_write_| here, as
  // it would cause a race:
  // 1. SetInt64() is called and pref is written to disk.
  // 2. SetInt64() is called and and the new value is queued.
  // 3. HandleFileChanged() is called regarding the initial write.
  FOR_EACH_OBSERVER(PrefsObserver, observers_, OnPrefChanged(name));
}

void Prefs::GetPrefStrings(const std::string& name,
                           bool read_all,
                           std::vector<PrefReadResult>* results) {
  CHECK(results);
  results->clear();

  for (std::vector<base::FilePath>::const_iterator iter = pref_paths_.begin();
       iter != pref_paths_.end(); ++iter) {
    base::FilePath path = iter->Append(name);
    std::string buf;

    // If there's a queued value that'll be written to the first path
    // soon, use it instead of reading from disk.
    if (iter == pref_paths_.begin() && prefs_to_write_.count(name))
      buf = prefs_to_write_[name];
    else if (!file_util::ReadFileToString(path, &buf))
      continue;

    TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
    PrefReadResult result;
    result.path = path.value();
    result.value = buf;
    results->push_back(result);
    if (!read_all)
      return;
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
       iter != results.end(); ++iter) {
    if (base::StringToInt64(iter->value, value))
      return true;
    else
      LOG(ERROR) << "Unable to parse int64 from " << iter->path;
  }
  return false;
}

bool Prefs::GetDouble(const std::string& name, double* value) {
  DCHECK(value);
  std::vector<PrefReadResult> results;
  GetPrefStrings(name, true, &results);

  for (std::vector<PrefReadResult>::const_iterator iter = results.begin();
       iter != results.end(); ++iter) {
    if (base::StringToDouble(iter->value, value))
      return true;
    else
      LOG(ERROR) << "Unable to parse double from " << iter->path;
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

void Prefs::SetString(const std::string& name, const std::string& value) {
  prefs_to_write_[name] = value;
  ScheduleWrite();
}

void Prefs::SetInt64(const std::string& name, int64 value) {
  prefs_to_write_[name] = base::Int64ToString(value);
  ScheduleWrite();
}

void Prefs::SetDouble(const std::string& name, double value) {
  prefs_to_write_[name] = base::DoubleToString(value);
  ScheduleWrite();
}

void Prefs::ScheduleWrite() {
  base::TimeDelta time_since_last_write =
      base::TimeTicks::Now() - last_write_time_;
  if (last_write_time_.is_null() || time_since_last_write >= write_interval_) {
    WritePrefs();
  } else if (!write_prefs_timeout_id_) {
    write_prefs_timeout_id_ = g_timeout_add(
        (write_interval_ - time_since_last_write).InMilliseconds(),
        HandleWritePrefsTimeoutThunk, this);
  }
}

void Prefs::WritePrefs() {
  CHECK(!pref_paths_.empty());
  for (std::map<std::string, std::string>::const_iterator it =
       prefs_to_write_.begin(); it != prefs_to_write_.end(); ++it) {
    const std::string& name = it->first;
    const std::string& value = it->second;
    base::FilePath path = pref_paths_[0].Append(name);
    if (file_util::WriteFile(path, value.data(), value.size()) == -1)
      PLOG(ERROR) << "Failed to write " << path.AsUTF8Unsafe();
  }
  prefs_to_write_.clear();
  last_write_time_ = base::TimeTicks::Now();
}

gboolean Prefs::HandleWritePrefsTimeout() {
  WritePrefs();
  write_prefs_timeout_id_ = 0;
  return FALSE;
}

}  // namespace power_manager
