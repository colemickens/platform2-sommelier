// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_MOCK_VPN_PROVIDER_H_
#define SHILL_VPN_MOCK_VPN_PROVIDER_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/vpn/vpn_provider.h"

namespace shill {

class MockVPNProvider : public VPNProvider {
 public:
  MockVPNProvider();
  ~MockVPNProvider() override;

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD3(OnDeviceInfoAvailable,
               bool(const std::string& link_name,
                    int interface_index,
                    Technology technology));
  MOCK_CONST_METHOD0(HasActiveService, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVPNProvider);
};

}  // namespace shill

#endif  // SHILL_VPN_MOCK_VPN_PROVIDER_H_
