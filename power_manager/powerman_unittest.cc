// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "metrics/metrics_library_mock.h"
#include "power_manager/mock_backlight.h"
#include "power_manager/powerman.h"

namespace power_manager {

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::Test;

static const int64 kRetrySuspendAttempts = 10;

class PowerManDaemonTest : public Test {
 public:
  PowerManDaemonTest()
      : prefs_(FilePath("config"), FilePath("config")),
        daemon_(&prefs_, &metrics_lib_, &backlight_, FilePath(".")) {}

  virtual void SetUp() {
    // Tests initialization done by the daemon's constructor.
    EXPECT_EQ(1, daemon_.lidstate_);
    EXPECT_EQ(0, daemon_.retry_suspend_count_);
    EXPECT_EQ(0, daemon_.lid_id_);
    EXPECT_EQ(0, daemon_.suspend_pid_);
  }

 protected:
  // Adds a metrics library mock expectation that the specified metric
  // will be generated.
  void ExpectMetric(const std::string& name, int sample,
                    int min, int max, int buckets) {
    EXPECT_CALL(metrics_lib_, SendToUMA(name, sample, min, max, buckets))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  PowerPrefs prefs_;
  // StrictMock turns all unexpected calls into hard failures.
  StrictMock<MetricsLibraryMock> metrics_lib_;
  StrictMock<MockBacklight> backlight_;
  PowerManDaemon daemon_;
};

TEST_F(PowerManDaemonTest, SendMetric) {
  ExpectMetric("Dummy.Metric", 25, 100, 200, 100);
  EXPECT_TRUE(daemon_.SendMetric("Dummy.Metric", 25, 100, 200, 100));
}

TEST_F(PowerManDaemonTest, GenerateRetrySuspendCountMetric) {
  // lid open, retries = 0
  daemon_.lidstate_ = PowerManDaemon::LID_STATE_OPENED;
  daemon_.GenerateMetricsOnResumeEvent();
  // lid open, retries > 0
  daemon_.retry_suspend_count_ = 3;
  daemon_.retry_suspend_attempts_ = kRetrySuspendAttempts;
  ExpectMetric(PowerManDaemon::kMetricRetrySuspendCountName, 3,
               PowerManDaemon::kMetricRetrySuspendCountMin,
               kRetrySuspendAttempts,
               PowerManDaemon::kMetricRetrySuspendCountBuckets);

  daemon_.GenerateMetricsOnResumeEvent();
}

}  // namespace power_manager
