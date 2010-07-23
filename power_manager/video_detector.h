// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_VIDEO_DETECTOR_H_
#define POWER_MANAGER_VIDEO_DETECTOR_H_

#include <gdk/gdkx.h>

#include "power_manager/video_detector_interface.h"

namespace power_manager {

class VideoDetector : public VideoDetectorInterface {
 public:
  VideoDetector();
  virtual ~VideoDetector() {}
  void Init();

  // GetVideoActivity should be called from OnIdleEvent when a transition to
  // idle state is imminent.
  // Sets |is_active| to true if video activity has been detected, otherwise
  // sets |is_active| to false.
  //
  // On success, returns true; otherwise return false.
  bool GetVideoActivity(bool* is_active);

 private:
  bool GetRootWindowProperty(Atom property, unsigned char** data);

  // Atom for the video time property of the root window.
  Atom video_time_atom_;

  Window root_window_;

  DISALLOW_COPY_AND_ASSIGN(VideoDetector);
};

}  // namespace power_manager

#endif // POWER_MANAGER_VIDEO_DETECTOR_H_
