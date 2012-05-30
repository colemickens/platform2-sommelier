// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/video_detector.h"

#include "base/logging.h"
#include "base/time.h"
#include "power_manager/util.h"

namespace power_manager {

VideoDetector::VideoDetector()
#ifdef USE_X11_ACTIVITY_DETECTION
    : video_time_atom_(None),
      root_window_(None)
#endif
    {}

void VideoDetector::Init() {
#ifdef USE_X11_ACTIVITY_DETECTION
  Display* display = util::GetDisplay();
  video_time_atom_ = XInternAtom(display, "_CHROME_VIDEO_TIME", false);
  DCHECK(video_time_atom_ != None);
  root_window_ = DefaultRootWindow(display);
  DCHECK(root_window_ != None);
#endif
}

bool VideoDetector::GetActivity(int64 activity_threshold_ms,
                                int64* time_since_activity_ms,
                                bool* is_active) {
  CHECK(NULL != is_active);
  if (GetTimeSinceVideoActivity(time_since_activity_ms)) {
    if (*time_since_activity_ms >= 0)  {
      *is_active = *time_since_activity_ms < activity_threshold_ms;
      LOG(INFO) << "Video activity " << (*is_active ? "found." : "not found.")
                << " Last timestamp: " << *time_since_activity_ms << "ms ago.";
    } else {
      *is_active = false;
      LOG(WARNING) << "Last video time is ahead of current time."
                   << " Check if the system clock has changed.";
    }
  } else {
    // This is not an error condition. Before wm detects the first video
    // activity, the property will not exist, and GetRootWindowProperty
    // will return false.
    LOG(INFO) << "Video activity not found, probably because "
              << "no video activity has been detected yet.";
    *is_active = false;
  }
  return true;
}

void VideoDetector::HandleActivity(const base::TimeTicks& last_activity_time) {
  last_video_time_ = last_activity_time;
}

bool VideoDetector::GetTimeSinceVideoActivity(int64* time_since_activity_ms) {
  if (!last_video_time_.is_null()) {
    *time_since_activity_ms =
        (base::TimeTicks::Now() - last_video_time_).InMilliseconds();
    return true;
  }
  bool result = false;
#ifdef USE_X11_ACTIVITY_DETECTION
  time_t* data = NULL;
  if (GetRootWindowProperty(video_time_atom_, (unsigned char**)&data)) {
    time_t seconds = time(NULL);
    time_t video_time = 0;
    DCHECK(NULL != data);
    video_time = data[0];
    if (seconds >= video_time)
      *time_since_activity_ms = (seconds - video_time) * 1000;
    result = true;
  }
  // Data may be a valid pointer even if GetRootWindowProperty returns false.
  // If the property returned isn't the type we asked, free it as well.
  if (data)
    XFree(data);
#endif
  return result;
}

#ifdef USE_X11_ACTIVITY_DETECTION
bool VideoDetector::GetRootWindowProperty(Atom property,
                                          unsigned char** data) {
  Atom actual_type;
  int32 actual_format = 0;
  unsigned long num_items = 0;
  unsigned long bytes_after_returned = 0;
  int32 status = 0;
  status = XGetWindowProperty(util::GetDisplay(),
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
#endif

}  // namespace power_manager
