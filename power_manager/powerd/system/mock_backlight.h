// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef POWER_MANAGER_POWERD_SYSTEM_MOCK_BACKLIGHT_H_
#define POWER_MANAGER_POWERD_SYSTEM_MOCK_BACKLIGHT_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "power_manager/powerd/system/backlight_interface.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace power_manager {
namespace system {

// TODO(derat): Remove this and replace it with a simple stub implementation.
class MockBacklight : public BacklightInterface {
 public:
  MockBacklight() {
    EXPECT_CALL(*this, AddObserver(NotNull())).WillRepeatedly(Return());
    EXPECT_CALL(*this, RemoveObserver(NotNull())).WillRepeatedly(Return());
  }

  MOCK_METHOD1(AddObserver, void(BacklightInterfaceObserver*));
  MOCK_METHOD1(RemoveObserver, void(BacklightInterfaceObserver*));

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

  MOCK_METHOD2(SetBrightnessLevel, bool(int64 level, base::TimeDelta interval));
  void ExpectSetBrightnessLevel(int64 level, bool ret_val) {
    EXPECT_CALL(*this, SetBrightnessLevel(level, _))
                .WillOnce(Return(ret_val))
                .RetiresOnSaturation();
  }

  void ExpectSetBrightnessLevelRepeatedly(int64 level, bool ret_val) {
    EXPECT_CALL(*this, SetBrightnessLevel(level, _))
                .WillRepeatedly(Return(ret_val));
  }

  void ExpectSetBrightnessLevelWithInterval(int64 level,
                                            base::TimeDelta interval,
                                            bool ret_val) {
    EXPECT_CALL(*this, SetBrightnessLevel(level, interval))
                .WillOnce(Return(ret_val))
                .RetiresOnSaturation();
  }

  MOCK_METHOD1(SetResumeBrightnessLevel, bool(int64 level));
  void ExpectSetResumeBrightnessLevel(int64 level) {
    EXPECT_CALL(*this, SetResumeBrightnessLevel(level))
                .WillRepeatedly(Return(true));
  }

};  // class MockBacklight

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_MOCK_BACKLIGHT_H_
