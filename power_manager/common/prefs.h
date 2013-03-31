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
#include "base/observer_list.h"
#include "base/time.h"
#include "power_manager/common/inotify.h"
#include "power_manager/common/signal_callback.h"

typedef int gboolean;
typedef unsigned int guint;

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
    Prefs* prefs_;  // not owned

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
  // Result of a pref file read operation.
  struct PrefReadResult {
    std::string value;  // The value that was read.
    std::string path;   // The pref file from which |value| was read.
  };

  // Called by |notifier_| when a pref is changed.  Notifies |observers_|.
  static gboolean HandleFileChangedThunk(const char* name,
                                         int watch_handle,
                                         unsigned int mask,
                                         gpointer data) {
    static_cast<Prefs*>(data)->HandleFileChanged(name);
    return TRUE;
  }
  void HandleFileChanged(const std::string& name);

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

  SIGNAL_CALLBACK_0(Prefs, gboolean, HandleWritePrefsTimeout);

  // List of file paths to read from, in order of precedence.
  // A value read from the first path will be used instead of values from the
  // other paths.
  std::vector<base::FilePath> pref_paths_;

  ObserverList<PrefsObserver> observers_;

  // For notification of updates to pref files.
  Inotify notifier_;

  // GLib timeout ID for calling HandleWritePrefsTimeout().
  guint write_prefs_timeout_id_;

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
