// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_MOCK_METRICS_STORE_H_
#define POWER_MANAGER_POWERD_MOCK_METRICS_STORE_H_
#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "power_manager/powerd/metrics_store.h"

using ::testing::Return;

namespace power_manager {

class MockMetricsStore : public MetricsStore {
 public:
  MOCK_METHOD0(Init, bool());
  void ExpectInit(bool ret_val) {
      EXPECT_CALL(*this, Init())
          .WillOnce(Return(ret_val))
          .RetiresOnSaturation();
  }

  MOCK_METHOD0(ResetNumOfSessionsPerChargeMetric, void());
  void ExpectResetNumOfSessionsPerChargeMetric() {
    EXPECT_CALL(*this, ResetNumOfSessionsPerChargeMetric())
        .Times(1)
        .RetiresOnSaturation();
  }

  MOCK_METHOD0(IncrementNumOfSessionsPerChargeMetric, void());
  void ExpectIncrementNumOfSessionsPerChargeMetric() {
    EXPECT_CALL(*this, IncrementNumOfSessionsPerChargeMetric())
        .Times(1)
        .RetiresOnSaturation();
  }

  MOCK_METHOD0(GetNumOfSessionsPerChargeMetric, int());
  void ExpectGetNumOfSessionsPerChargeMetric(int value) {
    EXPECT_CALL(*this, GetNumOfSessionsPerChargeMetric())
        .WillOnce(Return(value))
        .RetiresOnSaturation();
  }

  MOCK_CONST_METHOD0(IsInitialized, bool());
  void ExpectIsInitialized(bool ret_val) {
    EXPECT_CALL(*this, IsInitialized())
        .WillOnce(Return(ret_val))
        .RetiresOnSaturation();
  }
};  // class MockMetricsStore

}  // namespace power_manager
#endif  // POWER_MANAGER_POWERD_MOCK_METRICS_STORE_H_
