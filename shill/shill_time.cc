// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_time.h"

#include <time.h>

namespace shill {

namespace {

// As Time may be instantiated by MemoryLogMessage during a callback of
// AtExitManager, it needs to be a leaky singleton to avoid
// AtExitManager::RegisterCallback() from potentially being called within a
// callback of AtExitManager, which will lead to a crash. Making Time leaky is
// fine as it does not need to clean up or release any resource at destruction.
base::LazyInstance<Time>::Leaky g_time = LAZY_INSTANCE_INITIALIZER;

}  // namespace

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
