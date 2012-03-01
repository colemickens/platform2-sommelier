// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_VPN_DRIVER_
#define SHILL_MOCK_VPN_DRIVER_

#include <gmock/gmock.h>

#include "shill/vpn_driver.h"

namespace shill {

class MockVPNDriver : public VPNDriver {
 public:
  MockVPNDriver();
  virtual ~MockVPNDriver();

  MOCK_METHOD2(ClaimInterface, bool(const std::string &link_name,
                                    int interface_index));
  MOCK_METHOD1(Connect, void(Error *error));
};

}  // namespace shill

#endif  // SHILL_MOCK_VPN_DRIVER_
