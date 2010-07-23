// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_VIDEO_DETECTOR_INTERFACE_H_
#define POWER_MANAGER_VIDEO_DETECTOR_INTERFACE_H_

#include "base/basictypes.h"

namespace power_manager {

// Interface for detecting the presence of video during user idle periods.
class VideoDetectorInterface {
 public:
  // Sets |is_active| to true if video activity has been detected, otherwise
  // sets |is_active| to false.
  //
  // On success, returns true; otherwise return false.
  virtual bool GetVideoActivity(bool* is_active) = 0;

 protected:
  ~VideoDetectorInterface() {}
};

}  // namespace power_manager

#endif // POWER_MANAGER_VIDEO_DETECTOR_INTERFACE_H_
