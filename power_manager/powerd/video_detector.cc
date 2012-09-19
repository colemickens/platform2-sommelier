// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/video_detector.h"

#include "base/logging.h"
#include "base/time.h"
#include "power_manager/common/util.h"

namespace power_manager {

VideoDetector::VideoDetector() : is_fullscreen_(false) { }

void VideoDetector::Init() { }

bool VideoDetector::AddObserver(VideoDetectorObserver* observer) {
  if (observer == NULL) {
    LOG(WARNING) << "Attempted to add a NULL observer for VideoDetector!";
    return false;
  }
  if (observers_.insert(observer).second == false) {
    LOG(WARNING) << "Observer was already added to VideoDetector";
    return false;
  }
  return true;
}

bool VideoDetector::RemoveObserver(VideoDetectorObserver* observer) {
  if (observer == NULL) {
    LOG(WARNING) << "Attempted to remove a NULL observer for VideoDetector!";
    return false;
  }
  if(observers_.erase(observer) == 0) {
    LOG(WARNING) << "Observer was not present in VideoDetector";
    return false;
  }
  return true;
}

bool VideoDetector::GetActivity(int64 activity_threshold_ms,
                                int64* time_since_activity_ms,
                                bool* is_active) {
  CHECK(is_active);
  if (last_video_time_.is_null()) {
    // This is not an error condition.  It just means there has been no video
    // activity information passed from Chrome.
    LOG(INFO) << "Video activity not found, probably because "
              << "no video activity has been detected yet.";
    *is_active = false;
    return true;
  }
  *time_since_activity_ms =
      (base::TimeTicks::Now() - last_video_time_).InMilliseconds();
  if (*time_since_activity_ms >= 0)  {
    *is_active = *time_since_activity_ms < activity_threshold_ms;
    LOG(INFO) << "Video activity " << (*is_active ? "found." : "not found.")
              << " Last timestamp: " << *time_since_activity_ms << "ms ago.";
  } else {
    *is_active = false;
    // This should not happen due to clock jumps since we are using TimeTicks.
    LOG(WARNING) << "Last video time is ahead of current time.";
  }
  return true;
}

void VideoDetector::HandleActivity(const base::TimeTicks& last_activity_time) {
  last_video_time_ = last_activity_time;
  for (VideoDetectorObservers::iterator iter = observers_.begin();
       iter != observers_.end();
       ++iter) {
    VideoDetectorObserver* observer = *iter;
    observer->OnVideoDetectorEvent(last_activity_time, is_fullscreen_);
  }
}

void VideoDetector::HandleFullscreenChange(bool is_fullscreen) {
  is_fullscreen_ = is_fullscreen;
}

}  // namespace power_manager
