// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_VPN_PROVIDER_
#define SHILL_MOCK_VPN_PROVIDER_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/vpn_provider.h"

namespace shill {

class MockVPNProvider : public VPNProvider {
 public:
  MockVPNProvider();
  virtual ~MockVPNProvider();

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD2(OnDeviceInfoAvailable, bool(const std::string &link_name,
                                           int interface_index));

  DISALLOW_COPY_AND_ASSIGN(MockVPNProvider);
};

}  // namespace shill

#endif  // SHILL_MOCK_VPN_PROVIDER_
