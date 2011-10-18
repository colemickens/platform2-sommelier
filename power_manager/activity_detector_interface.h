// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_ACTIVITY_DETECTOR_INTERFACE_H_
#define POWER_MANAGER_ACTIVITY_DETECTOR_INTERFACE_H_

#include "base/basictypes.h"

namespace power_manager {

// Interface for detecting the presence of certain activity during user idle
// periods.
class ActivityDetectorInterface {
 public:
  // Sets |is_active| to true if audio activity has been detected, otherwise
  // sets |is_active| to false.  This is based on whether there has been any
  // activity detected within the last |activity_threshold_ms| ms.
  //
  // On success, returns true; Returns false if the audio state could not be
  // determined or the audio file could not be read
  virtual bool GetActivity(int64 activity_threshold_ms,
                           int64* time_since_activity_ms,
                           bool* is_active) = 0;

  // Turns activity detection on and off.  This is useful for cases where the
  // detection is polling-based, for example.
  virtual bool Enable() = 0;
  virtual bool Disable() = 0;

 protected:
  virtual ~ActivityDetectorInterface() {}
};

}  // namespace power_manager

#endif // POWER_MANAGER_ACTIVITY_DETECTOR_INTERFACE_H_
