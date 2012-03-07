// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_VPN_SERVICE_
#define SHILL_MOCK_VPN_SERVICE_

#include <gmock/gmock.h>

#include "shill/vpn_service.h"

namespace shill {

class MockVPNService : public VPNService {
 public:
  MockVPNService(ControlInterface *control,
                 EventDispatcher *dispatcher,
                 Metrics *metrics,
                 Manager *manager,
                 VPNDriver *driver);
  virtual ~MockVPNService();

  MOCK_METHOD1(SetState, void(ConnectState state));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVPNService);
};

}  // namespace shill

#endif  // SHILL_MOCK_VPN_SERVICE_
