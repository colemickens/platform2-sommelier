// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_BACKLIGHT_INTERFACE_H_
#define POWER_MANAGER_POWERD_SYSTEM_BACKLIGHT_INTERFACE_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/time.h"

namespace power_manager {
namespace system {

// Interface for classes that want to watch for changes to the backlight device
// (typically caused by a monitor getting plugged or unplugged).
class BacklightInterfaceObserver {
 public:
  BacklightInterfaceObserver() {}
  virtual ~BacklightInterfaceObserver() {}

  // Called when the underlying device has changed.  This generally means that
  // the available range of brightness levels (and likely also the current
  // level) has changed.
  virtual void OnBacklightDeviceChanged() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BacklightInterfaceObserver);
};

// Interface for getting and setting the backlight level from hardware.
class BacklightInterface {
 public:
  BacklightInterface() {}
  virtual ~BacklightInterface() {}

  // Adds or removes an observer.
  virtual void AddObserver(BacklightInterfaceObserver* observer) = 0;
  virtual void RemoveObserver(BacklightInterfaceObserver* observer) = 0;

  // Gets the maximum brightness level (in an an arbitrary device-specific
  // range; note that 0 is always the minimum allowable value, though).  If
  // this value changes, observers' OnBacklightDeviceChanged() methods will
  // be called.  Returns false on failure.
  virtual bool GetMaxBrightnessLevel(int64* max_level) = 0;

  // Gets the current brightness level (in an an arbitrary device-specific
  // range).  Returns false on failure.
  virtual bool GetCurrentBrightnessLevel(int64* current_level) = 0;

  // Sets the backlight to |level| over |interval|.  Returns false on failure.
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
