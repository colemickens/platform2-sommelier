// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_MOCK_VPN_SERVICE_H_
#define SHILL_VPN_MOCK_VPN_SERVICE_H_

#include <memory>

#include <gmock/gmock.h>

#include "shill/vpn/vpn_service.h"

namespace shill {

class MockVPNService : public VPNService {
 public:
  MockVPNService(Manager* manager, std::unique_ptr<VPNDriver> driver);
  ~MockVPNService() override;

  MOCK_METHOD(void, SetState, (ConnectState), (override));
  MOCK_METHOD(void, SetFailure, (ConnectFailure), (override));
  MOCK_METHOD(void, InitDriverPropertyStore, (), (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVPNService);
};

}  // namespace shill

#endif  // SHILL_VPN_MOCK_VPN_SERVICE_H_
