// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_VIDEO_DETECTOR_H_
#define POWER_MANAGER_VIDEO_DETECTOR_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <set>

#include "base/time.h"
#include "power_manager/activity_detector_interface.h"

namespace power_manager {

class VideoDetectorTest;

// Interface for observing video detection events.
class VideoDetectorObserver {
 public:
  // VideoDetector event handler. This handler is called whenever a new video
  // event is received by the VideoDetector.
  virtual void OnVideoDetectorEvent(int64 last_activity_time_ms) = 0;

 protected:
  virtual ~VideoDetectorObserver() {}
};

typedef std::set<VideoDetectorObserver*> VideoDetectorObservers;

class VideoDetector : public ActivityDetectorInterface {
 public:
  VideoDetector();
  virtual ~VideoDetector() {}
  void Init();

  // Register/unregister an VideoDetectorObserver. Return true when action
  // needed to be taken.
  bool AddObserver(VideoDetectorObserver* observer);
  bool RemoveObserver(VideoDetectorObserver* observer);

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

  // Call this to notify detector of video activity updates.  Stores the last
  // video activity time in |last_video_time_|.
  virtual void HandleActivity(const base::TimeTicks& last_activity_time);

 private:
  friend class VideoDetectorTest;
  FRIEND_TEST(VideoDetectorTest, AddObserver);
  FRIEND_TEST(VideoDetectorTest, AddObserverNULL);
  FRIEND_TEST(VideoDetectorTest, RemoveObserver);
  FRIEND_TEST(VideoDetectorTest, RemoveObserverNULL);
  FRIEND_TEST(VideoDetectorTest, HandleActivityObservers);
  FRIEND_TEST(VideoDetectorTest, HandleActivityNoObservers);

  // Timestamp of last known video time, based on Chrome notifications.
  base::TimeTicks last_video_time_;

  // Set of non-owned pointer to object listening for video detection events.
  VideoDetectorObservers observers_;

  DISALLOW_COPY_AND_ASSIGN(VideoDetector);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_VIDEO_DETECTOR_H_
