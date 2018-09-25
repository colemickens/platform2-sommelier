// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_dns_server_tester.h"

#include <string>
#include <vector>

#include "shill/connection.h"

namespace shill {

MockDnsServerTester::MockDnsServerTester(ConnectionRefPtr connection)
    : DnsServerTester(connection,
                      nullptr,
                      std::vector<std::string>(),
                      false,
                      base::Callback<void(const DnsServerTester::Status)>()) {}

MockDnsServerTester::~MockDnsServerTester() {}

}  // namespace shill
