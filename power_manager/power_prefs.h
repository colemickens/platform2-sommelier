// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_PREFS_H_
#define POWER_MANAGER_POWER_PREFS_H_

#include "base/file_path.h"
#include "power_manager/power_prefs_interface.h"

namespace power_manager {

class PowerPrefs : public PowerPrefsInterface {
 public:
  explicit PowerPrefs(const FilePath& pref_path, const FilePath& default_path);
  virtual ~PowerPrefs() {}

  // Overridden from PowerPrefsInterface:
  virtual bool ReadSetting(const char* setting_name, int64* val);
  virtual bool WriteSetting(const char* setting_name, int64 level);

 private:
  FilePath pref_path_;
  FilePath default_path_;

  DISALLOW_COPY_AND_ASSIGN(PowerPrefs);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_PREFS_H_
