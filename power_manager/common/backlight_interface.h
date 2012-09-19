// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_BACKLIGHT_INTERFACE_H_
#define POWER_MANAGER_COMMON_BACKLIGHT_INTERFACE_H_

#include "base/basictypes.h"
#include "base/logging.h"

namespace power_manager {

// Interface for classes that want to watch for changes to the backlight device
// (typically caused by a monitor getting plugged or unplugged).
class BacklightInterfaceObserver {
 public:
  BacklightInterfaceObserver() {}

  // Called when the underlying device has changed.  This generally means that
  // the available range of brightness levels (and likely also the current
  // level) has changed.
  virtual void OnBacklightDeviceChanged() = 0;

 protected:
  virtual ~BacklightInterfaceObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BacklightInterfaceObserver);
};

// Interface for getting and setting the backlight level from hardware.
class BacklightInterface {
 public:
  BacklightInterface() : observer_(NULL) {}

  void set_observer(BacklightInterfaceObserver* observer) {
    DCHECK(!observer || !observer_) << "Replacing existing observer";
    observer_ = observer;
  }

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

  BacklightInterfaceObserver* observer_;  // not owned

 private:
  DISALLOW_COPY_AND_ASSIGN(BacklightInterface);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_BACKLIGHT_INTERFACE_H_
