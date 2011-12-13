// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_BACKLIGHT_INTERFACE_H_
#define POWER_MANAGER_BACKLIGHT_INTERFACE_H_

#include "base/basictypes.h"

namespace power_manager {

// Interface for getting and setting the level of the backlight.
class BacklightInterface {
 public:
  // Set |level| to the current brightness level of the backlight, and set
  // |max| to the max brightness level of the backlight. The minimum brightness
  // level of the backlight is zero.
  //
  // On success, return true; otherwise return false.
  virtual bool GetBrightness(int64* level, int64* max) = 0;

  // Set the backlight to the specified brightness |level|.
  //
  // On success, return true; otherwise return false.
  virtual bool SetBrightness(int64 level) = 0;

 protected:
  virtual ~BacklightInterface() {}
};

}  // namespace power_manager

#endif  // POWER_MANAGER_BACKLIGHT_INTERFACE_H_
