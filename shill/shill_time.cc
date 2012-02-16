// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_time.h"

#include <time.h>

namespace shill {

// TODO(ers): not using LAZY_INSTANCE_INITIALIZER
// because of http://crbug.com/114828
static base::LazyInstance<Time> g_time = {0, {{0}}};

Time::Time() { }

Time::~Time() { }

Time* Time::GetInstance() {
  return g_time.Pointer();
}

int Time::GetTimeMonotonic(struct timeval *tv) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return -1;
  }

  tv->tv_sec = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec / 1000;
  return 0;
}

int Time::GetTimeOfDay(struct timeval *tv, struct timezone *tz) {
  return gettimeofday(tv, tz);
}

}  // namespace shill
