// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_TIME_H_
#define SHILL_TIME_H_

#include <sys/time.h>

#include <base/lazy_instance.h>

namespace shill {

// A "sys/time.h" abstraction allowing mocking in tests.
class Time {
 public:
  virtual ~Time();

  static Time *GetInstance();

  // gettimeofday
  virtual int GetTimeOfDay(struct timeval *tv, struct timezone *tz);

 protected:
  Time();

 private:
  friend struct base::DefaultLazyInstanceTraits<Time>;

  DISALLOW_COPY_AND_ASSIGN(Time);
};

}  // namespace shill

#endif  // SHILL_TIME_H_
