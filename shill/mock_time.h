// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_TIME_H_
#define SHILL_MOCK_TIME_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/shill_time.h"

namespace shill {

class MockTime : public Time {
 public:
  MockTime();
  virtual ~MockTime();

  MOCK_METHOD1(GetTimeMonotonic, int(struct timeval *tv));
  MOCK_METHOD2(GetTimeOfDay, int(struct timeval *tv, struct timezone *tz));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTime);
};

}  // namespace shill

#endif  // SHILL_MOCK_TIME_H_
