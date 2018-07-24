// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ICMP_H_
#define SHILL_MOCK_ICMP_H_

#include "shill/icmp.h"

#include <gmock/gmock.h>

#include "shill/net/ip_address.h"

namespace shill {

class MockIcmp : public Icmp {
 public:
  MockIcmp();
  ~MockIcmp() override;

  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(IsStarted, bool());
  MOCK_METHOD3(TransmitEchoRequest, bool(const IPAddress& destination,
                                         uint16_t id, uint16_t seq_num));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIcmp);
};

}  // namespace shill

#endif  // SHILL_MOCK_ICMP_H_
