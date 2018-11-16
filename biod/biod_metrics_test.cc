// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/macros.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <metrics/metrics_library_mock.h>

#include "biod/biod_metrics.h"

using ::testing::_;

namespace biod {
namespace {

class BiodMetricsTest : public testing::Test {
 protected:
  BiodMetricsTest() {
    biod_metrics_.SetMetricsLibraryForTesting(
        std::make_unique<MetricsLibraryMock>());
  }
  ~BiodMetricsTest() override = default;

  MetricsLibraryMock* GetMetricsLibraryMock() {
    return static_cast<MetricsLibraryMock*>(
        biod_metrics_.metrics_library_for_testing());
  }

  BiodMetrics biod_metrics_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BiodMetricsTest);
};

TEST_F(BiodMetricsTest, SendEnrolledFingerCount) {
  const int finger_count = 2;
  EXPECT_CALL(*GetMetricsLibraryMock(), SendEnumToUMA(_, finger_count, _))
      .Times(1);
  biod_metrics_.SendEnrolledFingerCount(finger_count);
}

TEST_F(BiodMetricsTest, SendFpUnlockEnabled) {
  EXPECT_CALL(*GetMetricsLibraryMock(), SendBoolToUMA(_, true)).Times(1);
  EXPECT_CALL(*GetMetricsLibraryMock(), SendBoolToUMA(_, false)).Times(1);
  biod_metrics_.SendFpUnlockEnabled(true);
  biod_metrics_.SendFpUnlockEnabled(false);
}

TEST_F(BiodMetricsTest, SendFpLatencyStatsOnMatch) {
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpMatchDurationCapture, _, _, _, _))
      .Times(1);
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpMatchDurationMatcher, _, _, _, _))
      .Times(1);
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpMatchDurationOverall, _, _, _, _))
      .Times(1);
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpNoMatchDurationCapture, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpNoMatchDurationMatcher, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpNoMatchDurationOverall, _, _, _, _))
      .Times(0);
  biod_metrics_.SendFpLatencyStats(true, 0, 0, 0);
}

TEST_F(BiodMetricsTest, SendFpLatencyStatsOnNoMatch) {
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpMatchDurationCapture, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpMatchDurationMatcher, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpMatchDurationOverall, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpNoMatchDurationCapture, _, _, _, _))
      .Times(1);
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpNoMatchDurationMatcher, _, _, _, _))
      .Times(1);
  EXPECT_CALL(*GetMetricsLibraryMock(),
              SendToUMA(metrics::kFpNoMatchDurationOverall, _, _, _, _))
      .Times(1);
  biod_metrics_.SendFpLatencyStats(false, 0, 0, 0);
}

TEST_F(BiodMetricsTest, SendFpLatencyStatsValues) {
  const int capture = 70;
  const int matcher = 187;
  const int overall = 223;
  EXPECT_CALL(*GetMetricsLibraryMock(), SendToUMA(_, capture, _, _, _))
      .Times(2);
  EXPECT_CALL(*GetMetricsLibraryMock(), SendToUMA(_, matcher, _, _, _))
      .Times(2);
  EXPECT_CALL(*GetMetricsLibraryMock(), SendToUMA(_, overall, _, _, _))
      .Times(2);
  biod_metrics_.SendFpLatencyStats(true, capture, matcher, overall);
  biod_metrics_.SendFpLatencyStats(false, capture, matcher, overall);
}

}  // namespace
}  // namespace biod
