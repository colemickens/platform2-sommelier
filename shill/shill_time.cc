// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_time.h"

namespace shill {

static base::LazyInstance<Time> g_time(base::LINKER_INITIALIZED);

Time::Time() { }

Time::~Time() { }

Time* Time::GetInstance() {
  return g_time.Pointer();
}

int Time::GetTimeOfDay(struct timeval *tv, struct timezone *tz) {
  return gettimeofday(tv, tz);
}

}  // namespace shill
