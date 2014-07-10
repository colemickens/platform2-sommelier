// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/rolling_average.h"

#include <cmath>

#include <base/logging.h>

namespace power_manager {
namespace system {

RollingAverage::RollingAverage(size_t window_size)
    : running_total_(0.0),
      window_size_(window_size) {
  DCHECK_GT(window_size_, static_cast<size_t>(0));
}

RollingAverage::~RollingAverage() {}

void RollingAverage::AddSample(double value, const base::TimeTicks& time) {
  if (!samples_.empty() && time < samples_.back().time) {
    LOG(WARNING) << "Sample " << value << "'s timestamp ("
                 << time.ToInternalValue() << ") precedes previously-"
                 << "appended sample's timestamp ("
                 << samples_.back().time.ToInternalValue() << ")";
  }

  while (samples_.size() >= window_size_)
    DeleteSample();
  running_total_ += value;
  samples_.push(Sample(value, time));
}

double RollingAverage::GetAverage() const {
  return samples_.empty() ? 0.0 : running_total_ / samples_.size();
}

base::TimeDelta RollingAverage::GetTimeDelta() const {
  return samples_.size() >= 2 ? samples_.back().time - samples_.front().time :
      base::TimeDelta();
}

double RollingAverage::GetValueDelta() const {
  return samples_.size() >= 2 ? samples_.back().value - samples_.front().value :
      0.0;
}

void RollingAverage::Clear() {
  running_total_ = 0.0;
  samples_ = std::queue<Sample>();
}

void RollingAverage::DeleteSample() {
  if (!samples_.empty()) {
    running_total_ -= samples_.front().value;
    samples_.pop();
  }
}

}  // namespace system
}  // namespace power_manager
