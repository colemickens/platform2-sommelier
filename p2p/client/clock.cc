// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client/clock.h"

#include <unistd.h>
#include <time.h>

namespace p2p {

namespace client {

unsigned int Clock::Sleep(unsigned int seconds) {
  return sleep(seconds);
}

base::Time Clock::GetMonotonicTime() {
  struct timespec now_ts;
  if (clock_gettime(CLOCK_MONOTONIC_RAW, &now_ts) != 0) {
    // Avoid logging this as an error as call-sites may call this very
    // often and we don't want to fill up the disk...
    return base::Time();
  }
  struct timeval now_tv;
  now_tv.tv_sec = now_ts.tv_sec;
  now_tv.tv_usec = now_ts.tv_nsec/base::Time::kNanosecondsPerMicrosecond;
  return base::Time::FromTimeVal(now_tv);
}

}  // namespace p2p

}  // namespace client
