// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_PREFS_H_
#define POWER_MANAGER_POWER_PREFS_H_

#include <glib.h>

#include <string>
#include <vector>

#include "base/file_path.h"
#include "power_manager/inotify.h"
#include "power_manager/power_prefs_interface.h"

namespace power_manager {

class PowerPrefs : public PowerPrefsInterface {
 public:
  explicit PowerPrefs(const FilePath& pref_path);
  explicit PowerPrefs(const std::vector<FilePath>& pref_paths);
  virtual ~PowerPrefs() {}

  bool StartPrefWatching(Inotify::InotifyCallback callback, gpointer data);

  // Overridden from PowerPrefsInterface:
  virtual bool GetString(const char* name, std::string* value);
  virtual bool GetInt64(const char* name, int64* value);
  virtual bool SetInt64(const char* name, int64 value);
  virtual bool GetDouble(const char* name, double* value);
  virtual bool SetDouble(const char* name, double value);
  virtual bool GetBool(const char* name, bool* value);

 private:
  // Result of a pref file read operation.
  struct PrefReadResult {
    std::string value;  // The value that was read.
    std::string path;   // The pref file from which |value| was read.
  };

  // Reads contents of pref files given by |name| from all the paths in
  // |pref_paths_| in order, where they exist.  Strips them of whitespace.
  // Stores each read result in |results|.
  // If |read_all| is true, it will attempt to read from all pref paths.
  // Otherwise it will return after successfully reading one pref file.
  void GetPrefStrings(const std::string& name,
                      bool read_all,
                      std::vector<PrefReadResult>* results);

  // List of file paths to read from, in order of precedence.
  // A value read from the first path will be used instead of values from the
  // other paths.
  std::vector<FilePath> pref_paths_;
  // For notification of updates to pref files.
  Inotify notifier_;
  DISALLOW_COPY_AND_ASSIGN(PowerPrefs);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_PREFS_H_
