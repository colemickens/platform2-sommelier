// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_dns_client.h"

#include <string>
#include <vector>

#include "shill/net/ip_address.h"

using std::string;
using std::vector;

namespace shill {

MockDnsClient::MockDnsClient()
    : DnsClient(IPAddress::kFamilyIPv4, "", vector<string>(), 0, nullptr,
                ClientCallback()) {}

MockDnsClient::~MockDnsClient() = default;

}  // namespace shill
