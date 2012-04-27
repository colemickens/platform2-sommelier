// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/logging.h"

#include "power_manager/rolling_average.h"

namespace power_manager {

RollingAverage::RollingAverage()
    : running_total_(0),
      max_window_size_(0) {
}

RollingAverage::~RollingAverage() {
}

void RollingAverage::Init(unsigned int max_window_size) {
  LOG_IF(WARNING,
         (sample_window_.size() != 0)
         ||  (running_total_ != 0)
         || (max_window_size_ == 0))
      << "Attempting to initialize RollingAverage when already initialized, "
      << "resetting instead!";
  Clear();
  max_window_size_ = max_window_size;
}

int64 RollingAverage::AddSample(int64 sample) {
  if (sample < 0) {
    LOG(ERROR) << "Received invalid sample of " << sample;
    return GetAverage();
  }

  if (IsFull())
    DeleteSample();
  InsertSample(sample);

  return GetAverage();
}

int64 RollingAverage::GetAverage() {
  if (sample_window_.empty())
    return 0;

  return lround((double)running_total_ / (double)sample_window_.size());
}

void RollingAverage::Clear() {
  running_total_ = 0;
  while (!sample_window_.empty())
    sample_window_.pop();
}

void RollingAverage::DeleteSample() {
  if(!sample_window_.empty()) {
    running_total_ -= sample_window_.front();
    sample_window_.pop();
  }
}

void RollingAverage::InsertSample(int64 sample) {
  LOG_IF(ERROR, sample_window_.size() >= max_window_size_)
      << "Inserting sample when max size already reached";

  running_total_ += sample;
  sample_window_.push(sample);
}

bool RollingAverage::IsFull() {
  LOG_IF(ERROR, sample_window_.size() > max_window_size_)
      << "Number of entries in sample window is greater then the max value";
  return (sample_window_.size() >= max_window_size_);
}

}  // namespace power_manager
