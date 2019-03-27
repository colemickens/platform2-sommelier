// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_MOCK_VPN_SERVICE_H_
#define SHILL_VPN_MOCK_VPN_SERVICE_H_

#include <gmock/gmock.h>

#include "shill/vpn/vpn_service.h"

namespace shill {

class MockVPNService : public VPNService {
 public:
  MockVPNService(Manager* manager, VPNDriver* driver);
  ~MockVPNService() override;

  MOCK_METHOD1(SetState, void(ConnectState state));
  MOCK_METHOD1(SetFailure, void(ConnectFailure failure));
  MOCK_METHOD0(InitDriverPropertyStore, void());
  MOCK_CONST_METHOD0(unloaded, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVPNService);
};

}  // namespace shill

#endif  // SHILL_VPN_MOCK_VPN_SERVICE_H_
