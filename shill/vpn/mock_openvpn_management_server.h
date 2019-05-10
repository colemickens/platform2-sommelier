// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_MOCK_OPENVPN_MANAGEMENT_SERVER_H_
#define SHILL_VPN_MOCK_OPENVPN_MANAGEMENT_SERVER_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "shill/vpn/openvpn_management_server.h"

namespace shill {

class MockOpenVPNManagementServer : public OpenVPNManagementServer {
 public:
  MockOpenVPNManagementServer();
  ~MockOpenVPNManagementServer() override;

  MOCK_METHOD2(Start,
               bool(Sockets* sockets,
                    std::vector<std::vector<std::string>>* options));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(ReleaseHold, void());
  MOCK_METHOD0(Hold, void());
  MOCK_METHOD0(Restart, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOpenVPNManagementServer);
};

}  // namespace shill

#endif  // SHILL_VPN_MOCK_OPENVPN_MANAGEMENT_SERVER_H_
