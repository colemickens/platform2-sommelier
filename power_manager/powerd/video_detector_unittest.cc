// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "power_manager/powerd/mock_video_detector_observer.h"
#include "power_manager/powerd/video_detector.h"

using ::testing::Test;
using ::testing::StrictMock;

namespace power_manager {

class VideoDetectorTest : public Test {
 public:
  VideoDetectorTest() {}

  virtual void SetUp() {}
  virtual void TearDown() {
    video_detector_.observers_.clear();
  }

  virtual bool IsPresent(VideoDetectorObserver* observer) {
    return (video_detector_.observers_.find(observer)
            != video_detector_.observers_.end());
  }

 protected:
  VideoDetector video_detector_;
  StrictMock<MockVideoDetectorObserver> observer_;
};  // class VideoDetectorTest

TEST_F(VideoDetectorTest, AddObserver) {
  // Adding the observer and testing it appears in the set.
  EXPECT_TRUE(video_detector_.AddObserver(&observer_));
  EXPECT_TRUE(IsPresent(&observer_));
  // Adding the same observer, this should fail, but the observer should still
  // be present in the set.
  EXPECT_FALSE(video_detector_.AddObserver(&observer_));
  EXPECT_TRUE(IsPresent(&observer_));
}

TEST_F(VideoDetectorTest, AddObserverNULL) {
  EXPECT_FALSE(video_detector_.AddObserver(NULL));
  EXPECT_TRUE(video_detector_.observers_.empty());
}

TEST_F(VideoDetectorTest, RemoveObserver) {
  // Seeding an observer to be removed.
  EXPECT_TRUE(video_detector_.AddObserver(&observer_));
  // Removing the observer, this should succeeded and leave the set empty.
  EXPECT_TRUE(video_detector_.RemoveObserver(&observer_));
  EXPECT_FALSE(IsPresent(&observer_));
  EXPECT_TRUE(video_detector_.observers_.empty());
  // Trying to remove the observer again, this should fail and the state of the
  // set should be the same.
  EXPECT_FALSE(video_detector_.RemoveObserver(&observer_));
  EXPECT_TRUE(video_detector_.observers_.empty());
}

TEST_F(VideoDetectorTest, RemoveObserverNULL) {
  EXPECT_FALSE(video_detector_.RemoveObserver(NULL));
  EXPECT_TRUE(video_detector_.observers_.empty());
}

TEST_F(VideoDetectorTest, HandleActivityObservers) {
  base::TimeTicks test_time = base::TimeTicks::Now();
  EXPECT_TRUE(video_detector_.AddObserver(&observer_));
  observer_.ExpectOnVideoDetectorEvent(test_time, false);
  video_detector_.HandleFullscreenChange(false);
  video_detector_.HandleActivity(test_time);
  EXPECT_TRUE(video_detector_.last_video_time_ == test_time);
}

TEST_F(VideoDetectorTest, HandleActivityNoObservers) {
  base::TimeTicks test_time = base::TimeTicks::Now();
  video_detector_.HandleActivity(test_time);
  EXPECT_TRUE(video_detector_.last_video_time_ == test_time);
}

}  // namespace power_manager
