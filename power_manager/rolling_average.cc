// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include "power_manager/rolling_average.h"

namespace power_manager {

RollingAverage::RollingAverage()
    : running_total_(0.0),
      max_window_size_(0) {
}

RollingAverage::~RollingAverage() {
}

void RollingAverage::Init(unsigned int max_window_size) {
  CHECK(sample_window_.size() == 0);
  CHECK(running_total_ == 0.0);
  CHECK(max_window_size_ == 0);

  max_window_size_ = max_window_size;
}

double RollingAverage::AddSample(double sample) {
  CHECK(sample >= 0.0);

  if (IsFull())
    DeleteSample();
  InsertSample(sample);

  return GetAverage();
}

double RollingAverage::GetAverage() {
  if (sample_window_.empty())
    return 0.0;

  return running_total_ / (double)sample_window_.size();
}

void RollingAverage::Clear() {
  running_total_ = 0.0;
  while (!sample_window_.empty())
    sample_window_.pop();
}

void RollingAverage::DeleteSample() {
  CHECK(!sample_window_.empty());

  running_total_ -= sample_window_.front();
  sample_window_.pop();
}

void RollingAverage::InsertSample(double sample) {
  CHECK(sample_window_.size() < max_window_size_);

  running_total_ += sample;
  sample_window_.push(sample);
}

bool RollingAverage::IsFull() {
  CHECK(max_window_size_ >= sample_window_.size());
  CHECK(max_window_size_ > 0);

  return (sample_window_.size() == max_window_size_);
}

}  // namespace power_manager
