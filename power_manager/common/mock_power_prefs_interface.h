// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_MOCK_POWER_PREFS_INTERFACE_H_
#define POWER_MANAGER_COMMON_MOCK_POWER_PREFS_INTERFACE_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "power_manager/common/power_prefs_interface.h"

namespace power_manager {

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;

class MockPowerPrefsInterface : public PowerPrefsInterface {
 public:
  MOCK_METHOD2(GetString,
      bool(const char* name, std::string* buf));
  void ExpectGetString(const char* name, std::string buf, bool ret_val) {
    EXPECT_CALL(*this, GetString(name, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(buf),
                        Return(ret_val)))
        .RetiresOnSaturation();
  }

  MOCK_METHOD2(GetInt64,
      bool(const char* name, int64* value));
  void ExpectGetInt64(const char* name, int64 value, bool ret_val) {
    EXPECT_CALL(*this, GetInt64(name, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(value),
                        Return(ret_val)))
        .RetiresOnSaturation();
  }

  MOCK_METHOD2(SetInt64,
      bool(const char* name, int64 value));
  void ExpectSetInt64(const char* name, int64 value, bool ret_val) {
    EXPECT_CALL(*this, SetInt64(name, value))
        .WillOnce(Return(ret_val))
        .RetiresOnSaturation();
  }

  MOCK_METHOD2(GetDouble,
      bool(const char* name, double* value));
  void ExpectGetDouble(const char* name, double value, bool ret_val) {
    EXPECT_CALL(*this, GetDouble(name, _))
        .WillOnce(DoAll(SetArgumentPointee<1>(value),
                        Return(ret_val)))
        .RetiresOnSaturation();
  }

  MOCK_METHOD2(SetDouble,
      bool(const char* name, double value));
  void ExpectSetDouble(const char* name, double value, bool ret_val) {
    EXPECT_CALL(*this, SetDouble(name, value))
        .WillOnce(Return(ret_val))
        .RetiresOnSaturation();
  }
};

}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_MOCK_POWER_PREFS_INTERFACE_H_
