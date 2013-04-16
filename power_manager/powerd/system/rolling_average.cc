// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/rolling_average.h"

#include <cmath>

#include "base/logging.h"

namespace power_manager {
namespace system {

RollingAverage::RollingAverage() : running_total_(0), window_size_(0) {}

RollingAverage::~RollingAverage() {}

void RollingAverage::Init(size_t window_size) {
  ChangeWindowSize(window_size);
}

void RollingAverage::ChangeWindowSize(size_t window_size) {
  DCHECK_GT(window_size, static_cast<size_t>(0));
  while (samples_.size() > window_size)
    DeleteSample();
  window_size_ = window_size;
}

int64 RollingAverage::AddSample(int64 sample) {
  DCHECK_GT(window_size_, static_cast<size_t>(0)) << "Must call Init()";
  if (sample < 0) {
    LOG(WARNING) << "Ignoring invalid sample of " << sample;
    return GetAverage();
  }

  while (samples_.size() >= window_size_)
    DeleteSample();
  running_total_ += sample;
  samples_.push(sample);
  return GetAverage();
}

int64 RollingAverage::GetAverage() {
  return samples_.empty() ? 0 :
      lround(static_cast<double>(running_total_ / samples_.size()));
}

void RollingAverage::Clear() {
  running_total_ = 0;
  while (!samples_.empty())
    samples_.pop();
}

void RollingAverage::DeleteSample() {
  if (!samples_.empty()) {
    running_total_ -= samples_.front();
    samples_.pop();
  }
}

}  // namespace system
}  // namespace power_manager
