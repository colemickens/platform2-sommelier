// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_PREFS_H_
#define POWER_MANAGER_POWER_PREFS_H_

#include "base/file_path.h"
#include "power_manager/power_prefs_interface.h"

namespace power_manager {

// Prefs for power manager
class PowerPrefs : public PowerPrefsInterface {
 public:
  explicit PowerPrefs(const FilePath& base_path);
  virtual ~PowerPrefs() {}

  // Read a setting from disk into |val|.
  // Returns true if successful; otherwise returns false.
  bool ReadSetting(const char* setting_name, int64 *val);

  // Write a setting to disk.
  // Returns true if successful; otherwise returns false.
  bool WriteSetting(const char* setting_name, int64 level);

 private:
  FilePath base_path_;

  DISALLOW_COPY_AND_ASSIGN(PowerPrefs);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_PREFS_H_
