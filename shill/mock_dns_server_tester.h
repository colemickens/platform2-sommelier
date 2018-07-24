// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DNS_SERVER_TESTER_H_
#define SHILL_MOCK_DNS_SERVER_TESTER_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/dns_server_tester.h"

namespace shill {

class MockDNSServerTester : public DNSServerTester {
 public:
  explicit MockDNSServerTester(ConnectionRefPtr connection);
  ~MockDNSServerTester() override;

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDNSServerTester);
};

}  // namespace shill

#endif  // SHILL_MOCK_DNS_SERVER_TESTER_H_
