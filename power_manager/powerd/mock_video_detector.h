// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_MOCK_VIDEO_DETECTOR_H_
#define POWER_MANAGER_POWERD_MOCK_VIDEO_DETECTOR_H_
#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "power_manager/powerd/video_detector.h"

namespace power_manager {

class MockVideoDetector : public VideoDetector {
 public:
  MOCK_METHOD3(GetActivity,
               bool(int64 activity_threshold_ms,
                    int64* time_since_activity_ms,
                    bool* is_active));
  MOCK_METHOD0(Enable, void(void));
  MOCK_METHOD0(Disable, void(void));
  MOCK_METHOD1(HandleActivity,
               void(const base::TimeTicks& last_activity_time));
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_MOCK_VIDEO_DETECTOR_H_
