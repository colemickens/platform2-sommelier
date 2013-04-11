// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/rolling_average.h"

#include <gtest/gtest.h>

#include "base/basictypes.h"

namespace power_manager {

TEST(RollingAverageTest, SingleSample) {
  RollingAverage average;
  average.Init(1);
  EXPECT_EQ(0, average.AddSample(0));
  EXPECT_EQ(5, average.AddSample(5));
  EXPECT_EQ(4, average.AddSample(4));
  EXPECT_EQ(0, average.AddSample(0));
  EXPECT_EQ(8, average.AddSample(8));
  EXPECT_EQ(8, average.GetAverage());

  // Negative samples should be ignored.
  EXPECT_EQ(8, average.AddSample(-1));
}

TEST(RollingAverageTest, MultipleSamples) {
  RollingAverage average;
  average.Init(3);
  EXPECT_EQ(4, average.AddSample(4));
  EXPECT_EQ(6, average.AddSample(8));
  EXPECT_EQ(8, average.AddSample(12));
  EXPECT_EQ(10, average.AddSample(10));
}

TEST(RollingAverageTest, ChangeWindowSize) {
  RollingAverage average;
  average.Init(2);
  EXPECT_EQ(5, average.AddSample(5));
  EXPECT_EQ(6, average.AddSample(7));
  average.ChangeWindowSize(4);
  EXPECT_EQ(6, average.GetAverage());
  EXPECT_EQ(7, average.AddSample(9));
  EXPECT_EQ(8, average.AddSample(11));
  average.ChangeWindowSize(2);
  EXPECT_EQ(10, average.GetAverage());
  average.ChangeWindowSize(1);
  EXPECT_EQ(11, average.GetAverage());
}

TEST(RollingAverageTest, Clear) {
  RollingAverage average;
  average.Init(2);
  EXPECT_EQ(3, average.AddSample(3));
  EXPECT_EQ(2, average.AddSample(1));
  average.Clear();
  EXPECT_EQ(0, average.GetAverage());
}

}  // namespace power_manager
