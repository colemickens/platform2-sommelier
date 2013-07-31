// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_CLIENT_FAKE_CLOCK_H__
#define P2P_CLIENT_FAKE_CLOCK_H__

#include "client/clock_interface.h"

#include <base/basictypes.h>

namespace p2p {

namespace client {

// Implements a fake version of the system time-related functions.
class FakeClock : public ClockInterface {
 public:
  FakeClock() : slept_seconds_(0), monotonic_time_(base::Time::Now()) {}

  virtual unsigned int Sleep(unsigned int seconds) {
    slept_seconds_ += seconds;
    monotonic_time_ += base::TimeDelta::FromSeconds(seconds);
    return 0;
  }

  virtual base::Time GetMonotonicTime() {
    return monotonic_time_;
  }

  unsigned int GetSleptTime() {
    return slept_seconds_;
  }

  void SetMonotonicTime(const base::Time &time) {
    monotonic_time_ = time;
  }

 private:
  unsigned int slept_seconds_;
  base::Time monotonic_time_;

  DISALLOW_COPY_AND_ASSIGN(FakeClock);
};

}  // namespace p2p

}  // namespace client

#endif  // P2P_CLIENT_FAKE_CLOCK_H__
