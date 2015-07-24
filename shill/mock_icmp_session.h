// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ICMP_SESSION_H_
#define SHILL_MOCK_ICMP_SESSION_H_

#include "shill/icmp_session.h"

#include <gmock/gmock.h>

#include "shill/net/ip_address.h"

namespace shill {

class MockIcmpSession : public IcmpSession {
 public:
  explicit MockIcmpSession(EventDispatcher* dispatcher);
  ~MockIcmpSession() override;

  MOCK_METHOD2(
      Start,
      bool(const IPAddress& destination,
           const IcmpSession::IcmpSessionResultCallback& result_callback));
  MOCK_METHOD0(Stop, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIcmpSession);
};

}  // namespace shill

#endif  // SHILL_MOCK_ICMP_SESSION_H_
