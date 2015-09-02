// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_FIREWALL_PROXY_H_
#define SHILL_MOCK_FIREWALL_PROXY_H_

#include "shill/firewall_proxy_interface.h"

#include <string>
#include <vector>

namespace shill {

class MockFirewallProxy : public FirewallProxyInterface {
 public:
  MockFirewallProxy() {}
  ~MockFirewallProxy() override {}

  MOCK_METHOD2(RequestVpnSetup, bool(const std::vector<std::string>& user_names,
                                     const std::string& interface));
  MOCK_METHOD0(RemoveVpnSetup, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFirewallProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_FIREWALL_PROXY_H_
