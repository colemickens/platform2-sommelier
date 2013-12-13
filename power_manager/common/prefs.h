// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_PREFS_H_
#define POWER_MANAGER_COMMON_PREFS_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/linked_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"

namespace power_manager {

class PrefsObserver;

// Interface for reading and writing preferences.
class PrefsInterface {
 public:
  virtual ~PrefsInterface() {}

  // Adds or removes an observer.
  virtual void AddObserver(PrefsObserver* observer) = 0;
  virtual void RemoveObserver(PrefsObserver* observer) = 0;

  // Reads settings and returns true on success.
  virtual bool GetString(const std::string& name, std::string* value) = 0;
  virtual bool GetInt64(const std::string& name, int64* value) = 0;
  virtual bool GetDouble(const std::string& name, double* value) = 0;
  virtual bool GetBool(const std::string& name, bool* value) = 0;

  // Writes settings (possibly asynchronously, although any deferred
  // changes will be reflected in Get*() calls).
  virtual void SetString(const std::string& name, const std::string& value) = 0;
  virtual void SetInt64(const std::string& name, int64 value) = 0;
  virtual void SetDouble(const std::string& name, double value) = 0;
};

// PrefsInterface implementation that reads and writes prefs from/to disk.
// Multiple directories are supported; this allows a default set of prefs
// to be placed on the readonly root partition and a second set of
// prefs under /var to be overlaid and changed at runtime.
class Prefs : public PrefsInterface {
 public:
  // Helper class for tests.
  class TestApi {
   public:
    explicit TestApi(Prefs* prefs);
    ~TestApi();

    void set_write_interval(base::TimeDelta interval) {
      prefs_->write_interval_ = interval;
    }

    // Calls HandleWritePrefsTimeout().  Returns false if the timeout
    // wasn't set.
    bool TriggerWriteTimeout();

   private:
    Prefs* prefs_;  // weak

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  Prefs();
  virtual ~Prefs();

  // Earlier directories in |pref_paths_| take precedence over later ones.  Only
  // the first directory is watched for changes.
  bool Init(const std::vector<base::FilePath>& pref_paths);

  // PrefsInterface implementation:
  virtual void AddObserver(PrefsObserver* observer) OVERRIDE;
  virtual void RemoveObserver(PrefsObserver* observer) OVERRIDE;
  virtual bool GetString(const std::string& name, std::string* value) OVERRIDE;
  virtual bool GetInt64(const std::string& name, int64* value) OVERRIDE;
  virtual bool GetDouble(const std::string& name, double* value) OVERRIDE;
  virtual bool GetBool(const std::string& name, bool* value) OVERRIDE;
  virtual void SetString(const std::string& name,
                         const std::string& value) OVERRIDE;
  virtual void SetInt64(const std::string& name, int64 value) OVERRIDE;
  virtual void SetDouble(const std::string& name, double value) OVERRIDE;

 private:
  typedef std::map<std::string, linked_ptr<base::FilePathWatcher> >
      FileWatcherMap;

  // Result of a pref file read operation.
  struct PrefReadResult {
    std::string value;  // The value that was read.
    std::string path;   // The pref file from which |value| was read.
  };

  // Called by |file_watcher_| when a pref is changed. Notifies |observers_|.
  void HandleFileChanged(const base::FilePath& path, bool error);

  // Reads contents of pref files given by |name| from all the paths in
  // |pref_paths_| in order, where they exist.  Strips them of whitespace.
  // Stores each read result in |results|.
  // If |read_all| is true, it will attempt to read from all pref paths.
  // Otherwise it will return after successfully reading one pref file.
  void GetPrefStrings(const std::string& name,
                      bool read_all,
                      std::vector<PrefReadResult>* results);

  // Calls WritePrefs() immediately if prefs haven't been written to disk
  // recently.  Otherwise, schedules HandleWritePrefsTimeout() if it isn't
  // already scheduled.
  void ScheduleWrite();

  // Writes |prefs_to_write_| to the first path in |pref_paths_|, updates
  // |last_write_time_|, and clears |prefs_to_write_|.
  void WritePrefs();

  // Updates |file_watchers_| to contain a watcher for every file currently in
  // |dir|.
  void UpdateFileWatchers(const base::FilePath& dir);

  // List of file paths to read from, in order of precedence.
  // A value read from the first path will be used instead of values from the
  // other paths.
  std::vector<base::FilePath> pref_paths_;

  ObserverList<PrefsObserver> observers_;

  // For notification of updates to pref files.
  base::FilePathWatcher dir_watcher_;

  // Map from pref file basenames to base::FilePathWatchers.
  FileWatcherMap file_watchers_;

  // Calls WritePrefs().
  base::OneShotTimer<Prefs> write_prefs_timer_;

  // Last time at which WritePrefs() was called.
  base::TimeTicks last_write_time_;

  // Minimum time between prefs getting written to disk.
  base::TimeDelta write_interval_;

  // Map from name to stringified value of prefs that need to be written to
  // the first path in |pref_paths_|.
  std::map<std::string, std::string> prefs_to_write_;

  DISALLOW_COPY_AND_ASSIGN(Prefs);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_PREFS_H_
