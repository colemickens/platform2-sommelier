// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MOCK_ACTIVITY_DETECTOR_H_
#define POWER_MANAGER_MOCK_ACTIVITY_DETECTOR_H_

#include <gmock/gmock.h>

#include "power_manager/activity_detector_interface.h"

namespace power_manager {

class MockActivityDetector : public ActivityDetectorInterface {
  public:
    MOCK_METHOD3(GetActivity, bool(int64, int64*, bool*));
    MOCK_METHOD0(Enable, void(void));
    MOCK_METHOD0(Disable, void(void));
};

}  // namespace power_manager

#endif  // POWER_MANAGER_MOCK_ACTIVITY_DETECTOR_H_
