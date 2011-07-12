// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_PREFS_H_
#define POWER_MANAGER_POWER_PREFS_H_

#include <glib.h>

#include "base/file_path.h"
#include "power_manager/inotify.h"
#include "power_manager/power_prefs_interface.h"

namespace power_manager {

class PowerPrefs : public PowerPrefsInterface {
 public:
  explicit PowerPrefs(const FilePath& pref_path, const FilePath& default_path);
  virtual ~PowerPrefs() {}

  bool StartPrefWatching(Inotify::InotifyCallback callback, gpointer data);

  // Overridden from PowerPrefsInterface:
  virtual bool GetInt64(const char* name, int64* value);
  virtual bool SetInt64(const char* name, int64 value);
  virtual bool GetDouble(const char* name, double* value);
  virtual bool SetDouble(const char* name, double value);

  const FilePath& pref_path() const { return pref_path_; }
  const FilePath& default_path() const { return default_path_; }

 private:
  FilePath pref_path_;
  FilePath default_path_;
  Inotify notifier_;

  DISALLOW_COPY_AND_ASSIGN(PowerPrefs);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_PREFS_H_
