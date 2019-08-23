// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_MOCK_OPENVPN_DRIVER_H_
#define SHILL_VPN_MOCK_OPENVPN_DRIVER_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/vpn/openvpn_driver.h"

namespace shill {

class MockOpenVPNDriver : public OpenVPNDriver {
 public:
  MockOpenVPNDriver();
  ~MockOpenVPNDriver() override;

  MOCK_METHOD1(OnReconnecting, void(ReconnectReason reason));
  MOCK_METHOD0(IdleService, void());
  MOCK_METHOD2(FailService,
               void(Service::ConnectFailure failure,
                    const std::string& error_details));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOpenVPNDriver);
};

}  // namespace shill

#endif  // SHILL_VPN_MOCK_OPENVPN_DRIVER_H_
