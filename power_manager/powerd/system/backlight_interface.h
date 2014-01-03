// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_BACKLIGHT_INTERFACE_H_
#define POWER_MANAGER_POWERD_SYSTEM_BACKLIGHT_INTERFACE_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/time/time.h"

namespace power_manager {
namespace system {

// Interface for getting and setting the backlight level from hardware.
class BacklightInterface {
 public:
  BacklightInterface() {}
  virtual ~BacklightInterface() {}

  // Gets the maximum brightness level (in an an arbitrary device-specific
  // range; note that 0 is always the minimum allowable value, though).
  virtual int64 GetMaxBrightnessLevel() = 0;

  // Gets the current brightness level (in an an arbitrary device-specific
  // range).
  virtual int64 GetCurrentBrightnessLevel() = 0;

  // Sets the backlight to |level| over |interval|. Returns false on failure.
  virtual bool SetBrightnessLevel(int64 level,
                                  base::TimeDelta interval) = 0;

  // Sets the resume backlight to |level|.  Returns false on failure.
  virtual bool SetResumeBrightnessLevel(int64 level) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BacklightInterface);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_BACKLIGHT_INTERFACE_H_
