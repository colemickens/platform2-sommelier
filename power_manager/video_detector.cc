// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/video_detector.h"

#include "base/logging.h"
#include "base/time.h"

namespace power_manager {

VideoDetector::VideoDetector()
    : video_time_atom_(None),
      root_window_(None) {}

void VideoDetector::Init() {
  video_time_atom_ = XInternAtom(GDK_DISPLAY(), "_CHROME_VIDEO_TIME", false);
  DCHECK(video_time_atom_ != None);
  root_window_ = DefaultRootWindow(GDK_DISPLAY());
  DCHECK(root_window_ != None);
}

bool VideoDetector::GetVideoActivity(int64 activity_threshold_ms,
                                     int64* time_since_activity_ms,
                                     bool* is_active) {
  time_t* data = NULL;
  CHECK(NULL != is_active);
  if (GetRootWindowProperty(video_time_atom_, (unsigned char**)&data)) {
    time_t seconds = time(NULL);
    time_t video_time = 0;
    DCHECK(NULL != data);
    video_time = data[0];
    if (seconds >= video_time)  {
      time_t time_since_activity = seconds - video_time;
      *time_since_activity_ms = time_since_activity * 1000;
      *is_active = *time_since_activity_ms < activity_threshold_ms;
      LOG(INFO) << "Video activity " << (*is_active ? "found." : "not found.")
                << " Last timestamp: " << time_since_activity << "s ago.";
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
  // Data may be a valid pointer even if GetRootWindowProperty returns false.
  // If the property returned isn't the type we asked, free it as well.
  if (data)
    XFree(data);
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
