// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DHCP_PROXY_H_
#define SHILL_MOCK_DHCP_PROXY_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/dhcp_proxy_interface.h"

namespace shill {

class MockDHCPProxy : public DHCPProxyInterface {
 public:
  MockDHCPProxy();
  virtual ~MockDHCPProxy();

  MOCK_METHOD1(Rebind, void(const std::string &interface));
  MOCK_METHOD1(Release, void(const std::string &interface));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDHCPProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_DHCP_PROXY_H_
