// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_ROLLING_AVERAGE_H_
#define POWER_MANAGER_ROLLING_AVERAGE_H_
#pragma once

#include <queue>

#include <base/basictypes.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace power_manager {

class RollingAverage {
 public:
  RollingAverage();
  virtual ~RollingAverage();

  virtual void Init(unsigned int max_window_size);

  virtual double AddSample(double sample);
  virtual double GetAverage();
  virtual void Clear();
 protected:
  void DeleteSample();
  void InsertSample(double sample);
  bool IsFull();

 private:
  friend class RollingAverageTest;
  FRIEND_TEST(RollingAverageTest, InitSuccess);
  FRIEND_TEST(RollingAverageTest, InitSamplePresent);
  FRIEND_TEST(RollingAverageTest, InitTotalNonZero);
  FRIEND_TEST(RollingAverageTest, InitWindowSizeSet);
  FRIEND_TEST(RollingAverageTest, AddSampleFull);
  FRIEND_TEST(RollingAverageTest, AddSampleEmpty);
  FRIEND_TEST(RollingAverageTest, AddSampleNegativeValue);
  FRIEND_TEST(RollingAverageTest, GetAverageFull);
  FRIEND_TEST(RollingAverageTest, GetAverageEmpty);
  FRIEND_TEST(RollingAverageTest, ClearSuccess);
  FRIEND_TEST(RollingAverageTest, DeleteSampleSuccess);
  FRIEND_TEST(RollingAverageTest, DeleteSampleEmpty);
  FRIEND_TEST(RollingAverageTest, InsertSampleSuccess);
  FRIEND_TEST(RollingAverageTest, InsertSampleFull);
  FRIEND_TEST(RollingAverageTest, IsFullFalse);
  FRIEND_TEST(RollingAverageTest, IsFullTrue);
  FRIEND_TEST(RollingAverageTest, IsFullUninitialized);
  FRIEND_TEST(RollingAverageTest, IsFullOverflow);

  std::queue<double> sample_window_;
  double running_total_;
  unsigned int max_window_size_;

  DISALLOW_COPY_AND_ASSIGN(RollingAverage);
};  // class RollingAverage

}  // namespace power_manager
#endif  // POWER_MANAGER_ROLLING_AVERAGE_H_
