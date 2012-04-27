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

 protected:
  RollingAverage rolling_average_;
};  // class RollingAverageTest

TEST_F(RollingAverageTest, InitSuccess) {
  rolling_average_.Init(kTestWindowSize);

  EXPECT_EQ(rolling_average_.sample_window_.size(), 0);
  EXPECT_EQ(rolling_average_.running_total_, 0);
  EXPECT_EQ(rolling_average_.max_window_size_, kTestWindowSize);
}

TEST_F(RollingAverageTest, InitSamplePresent) {
  rolling_average_.sample_window_.push(kTestSample);

  rolling_average_.Init(kTestWindowSize);

  EXPECT_EQ(rolling_average_.sample_window_.size(), 0);
  EXPECT_EQ(rolling_average_.running_total_, 0);
  EXPECT_EQ(rolling_average_.max_window_size_, kTestWindowSize);
}

TEST_F(RollingAverageTest, InitTotalNonZero) {
  rolling_average_.running_total_ = kTestSample;

  rolling_average_.Init(kTestWindowSize);

  EXPECT_EQ(rolling_average_.sample_window_.size(), 0);
  EXPECT_EQ(rolling_average_.running_total_, 0);
  EXPECT_EQ(rolling_average_.max_window_size_, kTestWindowSize);
}

TEST_F(RollingAverageTest, InitWindowSizeSet) {
  rolling_average_.max_window_size_ = kTestWindowSize;

  rolling_average_.Init(kTestWindowSize);

  EXPECT_EQ(rolling_average_.sample_window_.size(), 0);
  EXPECT_EQ(rolling_average_.running_total_, 0);
  EXPECT_EQ(rolling_average_.max_window_size_, kTestWindowSize);
}

TEST_F(RollingAverageTest, AddSampleFull) {
  rolling_average_.Init(kTestWindowSize);
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
  rolling_average_.Init(kTestWindowSize);
  EXPECT_EQ(rolling_average_.AddSample(kTestSample), kTestSample);
  EXPECT_EQ(rolling_average_.sample_window_.front(), kTestSample);
}

TEST_F(RollingAverageTest, AddSampleNegativeValue) {
  // Invalid samples should cause the current average to be returned and the
  // sample to be discarded.
  rolling_average_.Init(kTestWindowSize);
  EXPECT_EQ(rolling_average_.AddSample(kTestSample), kTestSample);
  EXPECT_EQ(rolling_average_.AddSample(-kTestSample), kTestSample);
}

TEST_F(RollingAverageTest, GetAverageFull) {
  rolling_average_.Init(kTestWindowSize);
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
  rolling_average_.Init(kTestWindowSize);
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
  rolling_average_.Init(kTestWindowSize);
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
  rolling_average_.Init(kTestWindowSize);

  rolling_average_.DeleteSample();
}

TEST_F(RollingAverageTest, InsertSampleSuccess) {
  rolling_average_.Init(kTestWindowSize);

  rolling_average_.InsertSample(kTestSample);
  EXPECT_EQ(rolling_average_.sample_window_.size(), 1);
  EXPECT_EQ(rolling_average_.running_total_, kTestSample);
  EXPECT_EQ(rolling_average_.sample_window_.front(), kTestSample);
}

TEST_F(RollingAverageTest, InsertSampleFull) {
  rolling_average_.Init(kTestWindowSize);

  for (unsigned int i = 0; i < kTestWindowSize; i++)
    rolling_average_.InsertSample(kTestSample);

  rolling_average_.InsertSample(kTestSample);

  // Inserting when full is an error, but not fatal
  EXPECT_EQ(rolling_average_.sample_window_.size(), kTestWindowSize + 1);
  EXPECT_EQ(rolling_average_.running_total_,
            kTestSample * (kTestWindowSize + 1));
}

TEST_F(RollingAverageTest, IsFullFalse) {
  rolling_average_.Init(kTestWindowSize);

  EXPECT_FALSE(rolling_average_.IsFull());
}

TEST_F(RollingAverageTest, IsFullTrue) {
  rolling_average_.Init(kTestWindowSize);
  for (unsigned int i = 0; i < kTestWindowSize; i++)
    rolling_average_.AddSample(kTestSample);

  EXPECT_TRUE(rolling_average_.IsFull());
}

TEST_F(RollingAverageTest, IsFullUninitialized) {
  EXPECT_TRUE(rolling_average_.IsFull());
}

TEST_F(RollingAverageTest, IsFullOverflow) {
  for(unsigned int i = 0; i < kTestWindowSize + 1; i++)
    rolling_average_.sample_window_.push(kTestSample);

  EXPECT_TRUE(rolling_average_.IsFull());
}

};  // namespace power_manager
