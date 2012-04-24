// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MOCK_ROLLING_AVERAGE_H_
#define POWER_MANAGER_MOCK_ROLLING_AVERAGE_H_
#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "power_manager/rolling_average.h"

namespace power_manager {

class MockRollingAverage : public RollingAverage {
 public:
  MOCK_METHOD1(Init, void(unsigned int max_window_size));
  void ExpectInit(unsigned int max_window_size) {
    EXPECT_CALL(*this, Init(max_window_size))
                .Times(1)
                .RetiresOnSaturation();
  }

  MOCK_METHOD1(AddSample, int64(int64 sample));
  void ExpectAddSample(int64 sample, int64 ret_val) {
    EXPECT_CALL(*this, AddSample(sample))
        .WillOnce(Return(ret_val))
        .RetiresOnSaturation();
  }

  MOCK_METHOD0(GetAverage, int64());
  void ExpectGetAverage(int64 ret_val) {
    EXPECT_CALL(*this, GetAverage())
        .WillOnce(Return(ret_val))
        .RetiresOnSaturation();
  }

  MOCK_METHOD0(Clear, void());
  void ExpectClear() {
    EXPECT_CALL(*this, Clear())
        .Times(1)
        .RetiresOnSaturation();
  }
};

}  // namespace power_manager
#endif  // POWER_MANAGER_MOCK_ROLLING_AVERAGE_H_
