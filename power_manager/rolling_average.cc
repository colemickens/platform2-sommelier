// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/logging.h"

#include "power_manager/rolling_average.h"

namespace power_manager {

RollingAverage::RollingAverage()
    : running_total_(0),
      current_window_size_(0) {
}

RollingAverage::~RollingAverage() {
}

void RollingAverage::Init(unsigned int window_size) {
  LOG_IF(WARNING,
         (sample_window_.size() != 0)
         ||  (running_total_ != 0))
      << "Attempting to initialize RollingAverage when already initialized, "
      << "resetting instead!";
  Clear();
  current_window_size_ = window_size;
}

void RollingAverage::ChangeWindowSize(unsigned int window_size) {
  if (window_size == 0) {
    LOG(ERROR) << "Called ChangeWindowSize with value of 0!";
    return;
  }

  if (current_window_size_ == window_size)
    return;

  LOG(INFO) << "ChangeWindowSize: from = " << current_window_size_
            << ", to = " << window_size;
  if (current_window_size_ > window_size) {
    while(sample_window_.size() > window_size)
      DeleteSample();
  }
  current_window_size_ = window_size;
}

int64 RollingAverage::AddSample(int64 sample) {
  if (sample < 0) {
    LOG(ERROR) << "Received invalid sample of " << sample;
    return GetAverage();
  }

  if (IsFull()) {
    int i = 0;
    while (IsFull()) {
      DeleteSample();
      ++i;
    }
    LOG_IF(WARNING, i > 1)
        << "Removed " << i - 1 << " extra samples when adding new sample value";
  }
  running_total_ += sample;
  sample_window_.push(sample);
  return GetAverage();
}

int64 RollingAverage::GetAverage() {
  if (sample_window_.empty())
    return 0;

  return lround(static_cast<double>(running_total_) /
                static_cast<double>(sample_window_.size()));
}

void RollingAverage::Clear() {
  running_total_ = 0;
  while (!sample_window_.empty())
    sample_window_.pop();
}

void RollingAverage::DeleteSample() {
  if (!sample_window_.empty()) {
    running_total_ -= sample_window_.front();
    sample_window_.pop();
  }
}

bool RollingAverage::IsFull() {
  LOG_IF(ERROR, sample_window_.size() > current_window_size_)
      << "Number of entries in sample window is greater than the current size!";
  return (sample_window_.size() >= current_window_size_);
}

}  // namespace power_manager
