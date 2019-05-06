// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_MOCK_TIME_H_
#define SHILL_NET_MOCK_TIME_H_

#include "shill/net/shill_time.h"

#include <base/macros.h>
#include <gmock/gmock.h>

namespace shill {

class MockTime : public Time {
 public:
  MockTime() = default;
  ~MockTime() override = default;

  MOCK_METHOD1(GetSecondsMonotonic, bool(time_t* seconds));
  MOCK_METHOD1(GetSecondsBoottime, bool(time_t* seconds));
  MOCK_METHOD1(GetTimeMonotonic, int(struct timeval* tv));
  MOCK_METHOD1(GetTimeBoottime, int(struct timeval* tv));
  MOCK_METHOD2(GetTimeOfDay, int(struct timeval* tv, struct timezone* tz));
  MOCK_METHOD0(GetNow, Timestamp());
  MOCK_CONST_METHOD0(GetSecondsSinceEpoch, time_t());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTime);
};

}  // namespace shill

#endif  // SHILL_NET_MOCK_TIME_H_
