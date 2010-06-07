// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_PREFS_INTERFACE_H_
#define POWER_MANAGER_POWER_PREFS_INTERFACE_H_

namespace power_manager {

// Prefs for power manager
class PowerPrefsInterface {
 public:
  // Read a setting from disk into |val|
  // Returns true if successful; otherwise returns false.
  virtual bool ReadSetting(const char* setting_name, int64* val) = 0;

  // Write a setting to disk.
  // Returns true if successful; otherwise returns false.
  virtual bool WriteSetting(const char* setting_name, int64 level) = 0;

 protected:
  ~PowerPrefsInterface() {}
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_PREFS_INTERFACE_H_
