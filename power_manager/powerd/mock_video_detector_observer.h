// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_MOCK_VIDEO_DETECTOR_OBSERVER_H_
#define POWER_MANAGER_POWERD_MOCK_VIDEO_DETECTOR_OBSERVER_H_
#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "power_manager/powerd/video_detector.h"

namespace power_manager {

class MockVideoDetectorObserver : public VideoDetectorObserver {
 public:
  MOCK_METHOD2(OnVideoDetectorEvent,
               void(base::TimeTicks last_activity_time,
                    bool is_fullscreen));
  void ExpectOnVideoDetectorEvent(base::TimeTicks last_activity_time,
                                  bool is_fullscreen) {
    EXPECT_CALL(*this, OnVideoDetectorEvent(last_activity_time,
                                            is_fullscreen))
                .Times(1)
                .RetiresOnSaturation();
  }
};  // class MockVideoDetectorObserver
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_MOCK_VIDEO_DETECTOR_OBSERVER_H_
