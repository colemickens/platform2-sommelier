// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/prefs.h"

#include <set>
#include <utility>

#include <base/bind.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <chromeos-config/libcros_config/cros_config.h>

#include "power_manager/common/prefs_observer.h"
#include "power_manager/common/util.h"

namespace power_manager {

namespace {

// Default directories where read/write and read-only preference files are
// stored.
constexpr char kReadWritePrefsDir[] = "/var/lib/power_manager";
constexpr char kReadOnlyPrefsDir[] = "/usr/share/power_manager";

// Subdirectory within the read-only prefs dir where board-specific prefs are
// stored.
constexpr char kBoardSpecificPrefsSubdir[] = "board_specific";

// Subdirectory within the read-only prefs dir where model-specific prefs are
// stored.
constexpr char kModelSpecificPrefsSubdir[] = "model_specific";

// Path and key name in CrosConfig database to look up model-specific pref
// subdirectory.
constexpr char kModelSubdirConfigPath[] = "/";
constexpr char kModelSubdirConfigKey[] = "powerd_prefs";

// Minimum time between batches of prefs being written to disk, in
// milliseconds.
const int kDefaultWriteIntervalMs = 1000;

// Returns a model-specific subdirectory of the read-only prefs dir if this
// model has one, or empty if not.
std::string GetModelSubdir() {
  brillo::CrosConfig config;
  if (!config.Init()) {
    // Not necessarily an error; not every model has a database yet.
    return "";
  }
  std::string subdir;
  if (!config.GetString(
          kModelSubdirConfigPath, kModelSubdirConfigKey, &subdir)) {
    // Not necessarily an error; not every model will need a subdir.
    return "";
  }

  return subdir;
}

}  // namespace

Prefs::TestApi::TestApi(Prefs* prefs) : prefs_(prefs) {}

Prefs::TestApi::~TestApi() {}

bool Prefs::TestApi::TriggerWriteTimeout() {
  if (!prefs_->write_prefs_timer_.IsRunning())
    return false;

  prefs_->write_prefs_timer_.Stop();
  prefs_->WritePrefs();
  return true;
}

Prefs::Prefs()
    : write_interval_(
          base::TimeDelta::FromMilliseconds(kDefaultWriteIntervalMs)) {}

Prefs::~Prefs() {
  if (write_prefs_timer_.IsRunning())
    WritePrefs();
}

// static
std::vector<base::FilePath> Prefs::GetDefaultPaths() {
  std::vector<base::FilePath> paths;
  paths.push_back(base::FilePath(kReadWritePrefsDir));

  const base::FilePath read_only_path(kReadOnlyPrefsDir);
  const std::string model_subdir = GetModelSubdir();
  if (!model_subdir.empty()) {
    paths.push_back(
        read_only_path.Append(kModelSpecificPrefsSubdir).Append(model_subdir));
  }

  paths.push_back(read_only_path.Append(kBoardSpecificPrefsSubdir));
  paths.push_back(read_only_path);
  return paths;
}

bool Prefs::Init(const std::vector<base::FilePath>& pref_paths) {
  CHECK(!pref_paths.empty());
  pref_paths_ = pref_paths;
  return dir_watcher_.Watch(
      pref_paths_[0],
      false,
      base::Bind(&Prefs::HandleFileChanged, base::Unretained(this)));
}

void Prefs::AddObserver(PrefsObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void Prefs::RemoveObserver(PrefsObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void Prefs::HandleFileChanged(const base::FilePath& path, bool error) {
  if (error) {
    LOG(ERROR) << "Got error while hearing about change to " << path.value();
    return;
  }

  // Rescan the directory if it changed.
  if (path == pref_paths_[0]) {
    UpdateFileWatchers(path);
    return;
  }

  const std::string name = path.BaseName().value();

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
       iter != pref_paths_.end();
       ++iter) {
    base::FilePath path = iter->Append(name);
    std::string buf;

    // If there's a queued value that'll be written to the first path
    // soon, use it instead of reading from disk.
    if (iter == pref_paths_.begin() && prefs_to_write_.count(name))
      buf = prefs_to_write_[name];
    else if (!base::ReadFileToString(path, &buf))
      continue;

    base::TrimWhitespaceASCII(buf, base::TRIM_TRAILING, &buf);
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

bool Prefs::GetInt64(const std::string& name, int64_t* value) {
  DCHECK(value);
  std::vector<PrefReadResult> results;
  GetPrefStrings(name, true, &results);

  for (std::vector<PrefReadResult>::const_iterator iter = results.begin();
       iter != results.end();
       ++iter) {
    if (base::StringToInt64(iter->value, value))
      return true;
    else
      LOG(ERROR) << "Unable to parse int64_t from " << iter->path;
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
      LOG(ERROR) << "Unable to parse double from " << iter->path;
  }
  return false;
}

bool Prefs::GetBool(const std::string& name, bool* value) {
  int64_t int_value = 0;
  if (!GetInt64(name, &int_value))
    return false;
  *value = int_value != 0;
  return true;
}

void Prefs::SetString(const std::string& name, const std::string& value) {
  prefs_to_write_[name] = value;
  ScheduleWrite();
}

void Prefs::SetInt64(const std::string& name, int64_t value) {
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
  } else if (!write_prefs_timer_.IsRunning()) {
    write_prefs_timer_.Start(FROM_HERE,
                             write_interval_ - time_since_last_write,
                             this,
                             &Prefs::WritePrefs);
  }
}

void Prefs::WritePrefs() {
  CHECK(!pref_paths_.empty());
  for (std::map<std::string, std::string>::const_iterator it =
           prefs_to_write_.begin();
       it != prefs_to_write_.end();
       ++it) {
    const std::string& name = it->first;
    const std::string& value = it->second;
    base::FilePath path = pref_paths_[0].Append(name);
    if (base::WriteFile(path, value.data(), value.size()) == -1)
      PLOG(ERROR) << "Failed to write " << path.AsUTF8Unsafe();
  }
  prefs_to_write_.clear();
  last_write_time_ = base::TimeTicks::Now();
}

void Prefs::UpdateFileWatchers(const base::FilePath& dir) {
  // Look for files that have been created or unlinked.
  base::FileEnumerator enumerator(dir, false, base::FileEnumerator::FILES);
  std::set<std::string> current_prefs;
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    current_prefs.insert(path.BaseName().value());
  }
  std::vector<std::string> added_prefs;
  for (std::set<std::string>::const_iterator it = current_prefs.begin();
       it != current_prefs.end();
       ++it) {
    if (file_watchers_.find(*it) == file_watchers_.end())
      added_prefs.push_back(*it);
  }
  std::vector<std::string> removed_prefs;
  for (FileWatcherMap::const_iterator it = file_watchers_.begin();
       it != file_watchers_.end();
       ++it) {
    if (current_prefs.find(it->first) == current_prefs.end())
      removed_prefs.push_back(it->first);
  }

  // Start watching new files.
  for (std::vector<std::string>::const_iterator it = added_prefs.begin();
       it != added_prefs.end();
       ++it) {
    const std::string& name = *it;
    const base::FilePath path = dir.Append(name);
    linked_ptr<base::FilePathWatcher> watcher(new base::FilePathWatcher);
    if (watcher->Watch(
            path,
            false,
            base::Bind(&Prefs::HandleFileChanged, base::Unretained(this)))) {
      file_watchers_.insert(std::make_pair(name, watcher));
    } else {
      LOG(ERROR) << "Unable to watch " << path.value() << " for changes";
    }
    FOR_EACH_OBSERVER(PrefsObserver, observers_, OnPrefChanged(name));
  }

  // Stop watching old files.
  for (std::vector<std::string>::const_iterator it = removed_prefs.begin();
       it != removed_prefs.end();
       ++it) {
    const std::string& name = *it;
    file_watchers_.erase(name);
    FOR_EACH_OBSERVER(PrefsObserver, observers_, OnPrefChanged(name));
  }
}

}  // namespace power_manager
