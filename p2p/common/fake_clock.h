// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_COMMON_FAKE_CLOCK_H__
#define P2P_COMMON_FAKE_CLOCK_H__

#include "common/clock_interface.h"

#include <base/basictypes.h>

namespace p2p {

namespace common {

// Implements a fake version of the system time-related functions.
class FakeClock : public ClockInterface {
 public:
  FakeClock() : monotonic_time_(base::Time::Now()) {}

  virtual void Sleep(const base::TimeDelta& duration) {
    slept_duration_ += duration;
    monotonic_time_ += duration;
  }

  virtual base::Time GetMonotonicTime() {
    return monotonic_time_;
  }

  base::TimeDelta GetSleptTime() {
    return slept_duration_;
  }

  void SetMonotonicTime(const base::Time &time) {
    monotonic_time_ = time;
  }

 private:
  base::TimeDelta slept_duration_;
  base::Time monotonic_time_;

  DISALLOW_COPY_AND_ASSIGN(FakeClock);
};

}  // namespace p2p

}  // namespace common

#endif  // P2P_COMMON_FAKE_CLOCK_H__
