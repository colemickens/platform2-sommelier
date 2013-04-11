// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_ROLLING_AVERAGE_H_
#define POWER_MANAGER_POWERD_ROLLING_AVERAGE_H_
#pragma once

#include <base/basictypes.h>

#include <queue>

namespace power_manager {

// This class tracks the rolling average from a continuous sequence of
// samples.
class RollingAverage {
 public:
  RollingAverage();
  ~RollingAverage();

  // Initializes the object to hold |window_size| samples.
  void Init(size_t window_size);

  // Changes the number of samples to hold.  Current samples are retained.
  void ChangeWindowSize(size_t window_size);

  // Adds a sample (which must be greater than or equal to zero) and
  // returns the new average.
  int64 AddSample(int64 sample);

  // Returns the current average.
  int64 GetAverage();

  // Clears all samples.
  void Clear();

 private:
  // Deletes the oldest sample.
  void DeleteSample();

  std::queue<int64> samples_;
  int64 running_total_;
  size_t window_size_;

  DISALLOW_COPY_AND_ASSIGN(RollingAverage);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_ROLLING_AVERAGE_H_
