// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef POWER_MANAGER_COMMON_MOCK_BACKLIGHT_H_
#define POWER_MANAGER_COMMON_MOCK_BACKLIGHT_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "power_manager/common/backlight_interface.h"

using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace power_manager {

class MockBacklight : public BacklightInterface {
 public:
  MOCK_METHOD1(GetCurrentBrightnessLevel, bool(int64* current_level));
  void ExpectGetCurrentBrightnessLevel(int64 current_level,
                                       bool ret_val) {
    EXPECT_CALL(*this, GetCurrentBrightnessLevel(NotNull()))
                .WillOnce(DoAll(SetArgumentPointee<0>(current_level),
                                Return(ret_val)))
                .RetiresOnSaturation();
  }

  MOCK_METHOD1(GetMaxBrightnessLevel, bool(int64* max_level));
  void ExpectGetMaxBrightnessLevel(int64 max_level,
                                   bool ret_val) {
    EXPECT_CALL(*this, GetMaxBrightnessLevel(NotNull()))
                .WillOnce(DoAll(SetArgumentPointee<0>(max_level),
                                Return(ret_val)))
                .RetiresOnSaturation();
  }

  MOCK_METHOD1(SetBrightnessLevel, bool(int64 level));
  void ExpectSetBrightnessLevel(int64 level,
                                bool ret_val) {
    EXPECT_CALL(*this, SetBrightnessLevel(level))
                .WillOnce(Return(ret_val))
                .RetiresOnSaturation();
  }
};  // class MockBacklight

}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_MOCK_BACKLIGHT_H_
