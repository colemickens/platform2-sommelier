// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/basictypes.h"
#include "power_manager/rolling_average.h"

using ::testing::Test;

namespace power_manager {

static const int64 kTestSample = 10;
static const unsigned int kTestWindowSize = 3;

class RollingAverageTest : public Test {
 public:
  RollingAverageTest() {}

  virtual void SetUp() {
    rolling_average_.Init(kTestWindowSize);
  }

  virtual void TearDown() {
    rolling_average_.Clear();
    rolling_average_.current_window_size_ = 0;
  }

 protected:
  RollingAverage rolling_average_;
};  // class RollingAverageTest

TEST_F(RollingAverageTest, InitSuccess) {
  TearDown();

  rolling_average_.Init(kTestWindowSize);

  EXPECT_EQ(rolling_average_.sample_window_.size(), 0);
  EXPECT_EQ(rolling_average_.running_total_, 0);
  EXPECT_EQ(rolling_average_.current_window_size_, kTestWindowSize);
}

TEST_F(RollingAverageTest, InitSamplePresent) {
  TearDown();
  rolling_average_.sample_window_.push(kTestSample);

  rolling_average_.Init(kTestWindowSize);

  EXPECT_EQ(rolling_average_.sample_window_.size(), 0);
  EXPECT_EQ(rolling_average_.running_total_, 0);
  EXPECT_EQ(rolling_average_.current_window_size_, kTestWindowSize);
}

TEST_F(RollingAverageTest, InitTotalNonZero) {
  TearDown();
  rolling_average_.running_total_ = kTestSample;

  rolling_average_.Init(kTestWindowSize);

  EXPECT_EQ(rolling_average_.sample_window_.size(), 0);
  EXPECT_EQ(rolling_average_.running_total_, 0);
  EXPECT_EQ(rolling_average_.current_window_size_, kTestWindowSize);
}

TEST_F(RollingAverageTest, InitCurrentWindowSizeSet) {
  TearDown();
  rolling_average_.current_window_size_ = kTestWindowSize;

  rolling_average_.Init(kTestWindowSize);

  EXPECT_EQ(rolling_average_.sample_window_.size(), 0);
  EXPECT_EQ(rolling_average_.running_total_, 0);
  EXPECT_EQ(rolling_average_.current_window_size_, kTestWindowSize);
}

TEST_F(RollingAverageTest, ChangeWindowSizeSame) {
  rolling_average_.ChangeWindowSize(kTestWindowSize);

  EXPECT_EQ(rolling_average_.current_window_size_, kTestWindowSize);
}

TEST_F(RollingAverageTest, ChangeWindowSizeGreater) {
  rolling_average_.current_window_size_ = kTestWindowSize / 2;
  rolling_average_.ChangeWindowSize((kTestWindowSize / 2) + 1);

  EXPECT_EQ(rolling_average_.current_window_size_, (kTestWindowSize / 2) + 1);
}

TEST_F(RollingAverageTest, ChangeWindowSizeLesser) {
  rolling_average_.ChangeWindowSize(1);

  EXPECT_EQ(rolling_average_.current_window_size_, 1);
}

TEST_F(RollingAverageTest, ChangeWindowSizeUnderflow) {
  rolling_average_.ChangeWindowSize(0);

  EXPECT_EQ(rolling_average_.current_window_size_, kTestWindowSize);
}

TEST_F(RollingAverageTest, AddSampleFull) {
  for (unsigned int i = 0; i < kTestWindowSize; i++)
    rolling_average_.sample_window_.push(0);

  EXPECT_EQ(rolling_average_.AddSample(kTestSample),
            (kTestSample / kTestWindowSize));

  EXPECT_EQ(kTestSample, rolling_average_.running_total_);
  for (unsigned int i = 0; i < kTestWindowSize - 1; i++) {
    EXPECT_EQ(rolling_average_.sample_window_.front(), 0);
    rolling_average_.sample_window_.pop();
  }
  EXPECT_EQ(rolling_average_.sample_window_.front(), kTestSample);
}

TEST_F(RollingAverageTest, AddSampleEmpty) {
  EXPECT_EQ(rolling_average_.AddSample(kTestSample), kTestSample);
  EXPECT_EQ(rolling_average_.sample_window_.front(), kTestSample);
}

TEST_F(RollingAverageTest, AddSampleNegativeValue) {
  // Invalid samples should cause the current average to be returned and the
  // sample to be discarded.
  EXPECT_EQ(rolling_average_.AddSample(kTestSample), kTestSample);
  EXPECT_EQ(rolling_average_.AddSample(-kTestSample), kTestSample);
}

TEST_F(RollingAverageTest, GetAverageFull) {
  for (unsigned int i = 0; i < kTestWindowSize; i++) {
    rolling_average_.sample_window_.push(kTestSample);
    rolling_average_.running_total_ += kTestSample;
  }

  EXPECT_EQ(rolling_average_.GetAverage(), kTestSample);
}

TEST_F(RollingAverageTest, GetAverageEmpty) {
  EXPECT_EQ(rolling_average_.GetAverage(), 0);
}

TEST_F(RollingAverageTest, ClearSuccess) {
  for (unsigned int i = 0; i < kTestWindowSize; i++) {
    rolling_average_.sample_window_.push(kTestSample);
    rolling_average_.running_total_ += kTestSample;
  }
  rolling_average_.Clear();

  EXPECT_EQ(rolling_average_.GetAverage(), 0);
  EXPECT_TRUE(rolling_average_.sample_window_.empty());
}

TEST_F(RollingAverageTest, DeleteSampleSuccess) {
  int64 expected_total = 0;
  for (unsigned int i = 0; i < kTestWindowSize; i++) {
    rolling_average_.sample_window_.push(1 + i);
    rolling_average_.running_total_ += 1 + i;
    expected_total += 1 + i;
  }

  rolling_average_.DeleteSample();

  expected_total -= 1;
  EXPECT_EQ(rolling_average_.running_total_, expected_total);
  EXPECT_EQ(rolling_average_.sample_window_.size(), kTestWindowSize - 1);
  for (unsigned int i = 0; i < kTestWindowSize - 1; i++) {
    EXPECT_EQ(rolling_average_.sample_window_.front(), 2 + i);
    rolling_average_.sample_window_.pop();
  }
}

TEST_F(RollingAverageTest, DeleteSampleEmpty) {
  rolling_average_.DeleteSample();
}

TEST_F(RollingAverageTest, IsFullFalse) {
  EXPECT_FALSE(rolling_average_.IsFull());
}

TEST_F(RollingAverageTest, IsFullTrue) {
  for (unsigned int i = 0; i < kTestWindowSize; i++)
    rolling_average_.AddSample(kTestSample);

  EXPECT_TRUE(rolling_average_.IsFull());
}

TEST_F(RollingAverageTest, IsFullUninitialized) {
  TearDown();
  EXPECT_TRUE(rolling_average_.IsFull());
}

TEST_F(RollingAverageTest, IsFullOverflow) {
  for (unsigned int i = 0; i < kTestWindowSize + 1; i++)
    rolling_average_.sample_window_.push(kTestSample);

  EXPECT_TRUE(rolling_average_.IsFull());
}

};  // namespace power_manager
