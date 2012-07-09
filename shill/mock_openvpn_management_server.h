// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_OPENVPN_MANAGEMENT_SERVER_
#define SHILL_MOCK_OPENVPN_MANAGEMENT_SERVER_

#include <gmock/gmock.h>

#include "shill/openvpn_management_server.h"

namespace shill {

class MockOpenVPNManagementServer : public OpenVPNManagementServer {
 public:
  MockOpenVPNManagementServer();
  virtual ~MockOpenVPNManagementServer();

  MOCK_METHOD3(Start, bool(EventDispatcher *dispatcher,
                           Sockets *sockets,
                           std::vector<std::string> *options));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(ReleaseHold, void());
  MOCK_METHOD0(Hold, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOpenVPNManagementServer);
};

}  // namespace shill

#endif  // SHILL_MOCK_OPENVPN_MANAGEMENT_SERVER_
