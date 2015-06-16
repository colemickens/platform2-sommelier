// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DNS_CLIENT_H_
#define SHILL_MOCK_DNS_CLIENT_H_

#include <string>

#include "shill/dns_client.h"

#include <base/macros.h>
#include <gmock/gmock.h>

namespace shill {

class MockDNSClient : public DNSClient {
 public:
  MockDNSClient();
  ~MockDNSClient() override;

  MOCK_METHOD2(Start, bool(const std::string& hostname, Error* error));
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(IsActive, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDNSClient);
};

}  // namespace shill

#endif  // SHILL_MOCK_DNS_CLIENT_H_
