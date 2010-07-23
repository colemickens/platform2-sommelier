// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/video_detector.h"

#include "base/logging.h"
#include "base/time.h"

namespace power_manager {

// The maximum interval between the last time of video activity, and the time
// of idle event. If a video event occurs within this threshold of an idle
// event, defer the idle.
static const time_t kVideoTimeThresholdS = 10;

VideoDetector::VideoDetector()
    : video_time_atom_(None),
      root_window_(NULL) {}

void VideoDetector::Init() {
  video_time_atom_ = XInternAtom(GDK_DISPLAY(), "_CHROME_VIDEO_TIME", true);
  DCHECK(None != video_time_atom_);
  root_window_ = DefaultRootWindow(GDK_DISPLAY());
  DCHECK(root_window_);
}

bool VideoDetector::GetVideoActivity(bool* is_active) {
  time_t* data = NULL;
  CHECK(NULL != is_active);
  if (GetRootWindowProperty(video_time_atom_, (unsigned char**)&data)) {
    time_t seconds = time(NULL);
    time_t video_time = 0;
    DCHECK(NULL != data);
    video_time = data[0];
    if (seconds >= video_time)  {
      *is_active = seconds - video_time < kVideoTimeThresholdS;
      LOG(INFO) << "Video activity " << (*is_active ? "found." : "not found.")
                << " Last timestamp: " << seconds - video_time << "s ago.";
    } else {
      *is_active = false;
      LOG(WARNING) << "_CHROME_VIDEO_TIME is ahead of time(NULL)."
                   << " Check if the system clock has changed.";
    }
  } else {
    // This is not an error condition. Before wm detects the first video
    // activity, the property will not exist, and GetRootWindowProperty
    // will return false.
    LOG(INFO) << "Video activity not found."
              << " Did not get _CHROME_VIDEO_TIME from root window.";
    *is_active = false;
  }
  return true;
}

bool VideoDetector::GetRootWindowProperty(Atom property,
                                          unsigned char** data) {
  Atom actual_type;
  int32 actual_format = 0;
  unsigned long num_items = 0;
  unsigned long bytes_after_returned = 0;
  int32 status = 0;
  status = XGetWindowProperty(GDK_DISPLAY(),
                              root_window_,
                              property,
                              0,                // zero word offset.
                              1,                // # of 32-bit quantities
                              false,            // Do not delete property
                              property,         // Request specific property
                              &actual_type,
                              &actual_format,
                              &num_items,
                              &bytes_after_returned,
                              data);
  return (Success == status && property == actual_type);
}

}  // namespace power_manager
