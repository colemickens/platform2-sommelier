// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_PREFS_INTERFACE_H_
#define POWER_MANAGER_POWER_PREFS_INTERFACE_H_

#include "base/basictypes.h"

namespace power_manager {

// Interface for reading and writing the preferences for power manager.
class PowerPrefsInterface {
 public:
  // Read a setting from disk into |value|.
  // Returns true if successful; otherwise returns false.
  virtual bool GetInt64(const char* name, int64* value) = 0;

  // Write a setting to disk.
  // Returns true if successful; otherwise returns false.
  virtual bool SetInt64(const char* name, int64 value) = 0;

  // Read a setting from disk into |value|.
  // Returns true if successful; otherwise returns false.
  virtual bool GetDouble(const char* name, double* value) = 0;

  // Write a setting to disk.
  // Returns true if successful; otherwise returns false.
  virtual bool SetDouble(const char* name, double value) = 0;

 protected:
  virtual ~PowerPrefsInterface() {}
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_PREFS_INTERFACE_H_
