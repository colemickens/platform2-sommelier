// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_VIDEO_DETECTOR_H_
#define POWER_MANAGER_VIDEO_DETECTOR_H_

#include <gdk/gdkx.h>

#include "power_manager/activity_detector_interface.h"

namespace power_manager {

class VideoDetector : public ActivityDetectorInterface {
 public:
  VideoDetector();
  virtual ~VideoDetector() {}
  void Init();

  // GetActivity should be called from OnIdleEvent when a transition to idle
  // state is imminent.
  // Sets |is_active| to true if video activity has been detected, otherwise
  // sets |is_active| to false.
  //
  // On success, returns true; otherwise return false.
  virtual bool GetActivity(int64 activity_threshold_ms,
                           int64* time_since_activity_ms,
                           bool* is_active);

  // These are not used by the video detector, which is not poll-driven.
  virtual bool Enable() { return true; }
  virtual bool Disable() { return true; }

 private:
  bool GetRootWindowProperty(Atom property, unsigned char** data);

  // Atom for the video time property of the root window.
  Atom video_time_atom_;

  Window root_window_;

  DISALLOW_COPY_AND_ASSIGN(VideoDetector);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_VIDEO_DETECTOR_H_
