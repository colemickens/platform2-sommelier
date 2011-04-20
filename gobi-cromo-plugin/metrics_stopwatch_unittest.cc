// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/scoped_ptr.h>
#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "metrics_stopwatch.h"

using ::testing::AllOf;
using ::testing::Gt;
using ::testing::Lt;
using ::testing::StrEq;
using ::testing::_;

class MockMetricsServer : public MetricsLibraryInterface {
 public:
  MOCK_METHOD0(Init,
      void());
  MOCK_METHOD5(SendToUMA,
      bool(const std::string& name, int sample, int min, int max, int nbuckets));
  MOCK_METHOD3(SendEnumToUMA,
      bool(const std::string& name, int sample, int max));
  MOCK_METHOD1(SendUserActionToUMA,
      bool(const std::string& action));
};

class MetricsStopwatchTest : public ::testing::Test {
 public:
  MetricsStopwatchTest() :
      s("Test", 0, 2000, 5) {
  }
  void SetUp() {
    metrics_ = new MockMetricsServer;
    s.SetMetrics(metrics_);
  }

  MockMetricsServer *metrics_;
  MetricsStopwatch s;
};

TEST_F(MetricsStopwatchTest, MetricsStopwatchSleep) {
  const int kTarget = 250;
  EXPECT_CALL(*metrics_, SendToUMA(StrEq("Test"),
                                   AllOf(Gt(kTarget / 3), Lt(kTarget * 3)),
                                   0,
                                   2000,
                                   5));
  s.Start();
  usleep(kTarget * 1000);
  s.Stop();
}

TEST_F(MetricsStopwatchTest, SetRegularOrder) {
  EXPECT_CALL(*metrics_, SendToUMA(StrEq("Test"),
                                   75,
                                   0,
                                   2000,
                                   5));
  s.SetStart(1ULL << 32);
  s.SetStop((1ULL << 32) + 75);
}


TEST_F(MetricsStopwatchTest, SetBackwardsAndReset) {
  EXPECT_CALL(*metrics_, SendToUMA(StrEq("Test"),
                                   75,
                                   0,
                                   2000,
                                   5));

  s.SetStart(1);
  s.Reset();
  s.SetStop((1ULL << 32) + 75);
  s.SetStart(1ULL << 32);
}

TEST_F(MetricsStopwatchTest, OnlyStop) {
  // Don't expect any calls
  s.Stop();
}

TEST_F(MetricsStopwatchTest, OnlyStopIfStarted) {
  // Don't expect any calls
  s.StopIfStarted();
}

TEST_F(MetricsStopwatchTest, StopIfStarted) {
  EXPECT_CALL(*metrics_, SendToUMA(StrEq("Test"),
                                   _,
                                   0,
                                   2000,
                                   5));
  s.Start();
  s.StopIfStarted();
}

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  google::InitGoogleLogging(argv[0]);
  return RUN_ALL_TESTS();
}
