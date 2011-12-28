// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_BACKLIGHT_INTERFACE_H_
#define POWER_MANAGER_BACKLIGHT_INTERFACE_H_

#include "base/basictypes.h"

namespace power_manager {

// Interface for getting and setting the backlight level from hardware.
class BacklightInterface {
 public:
  // Get the maximum brightness level (in an an arbitrary device-specific range;
  // note that 0 is always the minimum allowable value, though).  This value
  // never changes.  Returns false on failure.
  virtual bool GetMaxBrightnessLevel(int64* max_level) = 0;

  // Get the current brightness level (in an an arbitrary device-specific
  // range).  Returns false on failure.
  virtual bool GetCurrentBrightnessLevel(int64* current_level) = 0;

  // Set the backlight to |level|.  Returns false on failure.
  virtual bool SetBrightnessLevel(int64 level) = 0;

 protected:
  virtual ~BacklightInterface() {}
};

}  // namespace power_manager

#endif  // POWER_MANAGER_BACKLIGHT_INTERFACE_H_
