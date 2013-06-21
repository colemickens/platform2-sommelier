// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/rolling_average.h"

#include <gtest/gtest.h>

#include "base/basictypes.h"

namespace power_manager {
namespace system {

TEST(RollingAverageTest, SingleSample) {
  RollingAverage average;
  average.Init(1);
  EXPECT_DOUBLE_EQ(0.0, average.AddSample(0.0));
  EXPECT_DOUBLE_EQ(5.0, average.AddSample(5.0));
  EXPECT_DOUBLE_EQ(4.0, average.AddSample(4.0));
  EXPECT_DOUBLE_EQ(0.0, average.AddSample(0.0));
  EXPECT_DOUBLE_EQ(8.0, average.AddSample(8.0));
  EXPECT_DOUBLE_EQ(8.0, average.GetAverage());

  // Negative samples should be ignored.
  EXPECT_DOUBLE_EQ(8.0, average.AddSample(-1.0));
}

TEST(RollingAverageTest, MultipleSamples) {
  RollingAverage average;
  average.Init(3);
  EXPECT_DOUBLE_EQ(4.0, average.AddSample(4.0));
  EXPECT_DOUBLE_EQ(6.0, average.AddSample(8.0));
  EXPECT_DOUBLE_EQ(8.0, average.AddSample(12.0));
  EXPECT_DOUBLE_EQ(10.0, average.AddSample(10.0));
}

TEST(RollingAverageTest, ChangeWindowSize) {
  RollingAverage average;
  average.Init(2);
  EXPECT_DOUBLE_EQ(5.0, average.AddSample(5.0));
  EXPECT_DOUBLE_EQ(6.0, average.AddSample(7.0));
  average.ChangeWindowSize(4);
  EXPECT_DOUBLE_EQ(6.0, average.GetAverage());
  EXPECT_DOUBLE_EQ(7.0, average.AddSample(9.0));
  EXPECT_DOUBLE_EQ(8.0, average.AddSample(11.0));
  average.ChangeWindowSize(2);
  EXPECT_DOUBLE_EQ(10.0, average.GetAverage());
  average.ChangeWindowSize(1);
  EXPECT_DOUBLE_EQ(11.0, average.GetAverage());
}

TEST(RollingAverageTest, Clear) {
  RollingAverage average;
  average.Init(2);
  EXPECT_DOUBLE_EQ(3.0, average.AddSample(3.0));
  EXPECT_DOUBLE_EQ(2.5, average.AddSample(2.0));
  average.Clear();
  EXPECT_DOUBLE_EQ(0.0, average.GetAverage());
}

}  // namespace system
}  // namespace power_manager
