// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>
#include "shill/mock_dns_server_tester.h"
#include "shill/connection.h"

namespace shill {

MockDNSServerTester::MockDNSServerTester(ConnectionRefPtr connection)
    : DNSServerTester(connection,
                      NULL,
                      std::vector<std::string>(),
                      false,
                      base::Callback<void(const DNSServerTester::Status)>()) {}

MockDNSServerTester::~MockDNSServerTester() {}

}  // namespace shill
