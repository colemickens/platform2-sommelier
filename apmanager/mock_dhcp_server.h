// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_DHCP_SERVER_H_
#define APMANAGER_MOCK_DHCP_SERVER_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "apmanager/dhcp_server.h"

namespace apmanager {

class MockDHCPServer : public DHCPServer {
 public:
  MockDHCPServer();
  ~MockDHCPServer() override;

  MOCK_METHOD0(Start, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDHCPServer);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_DHCP_SERVER_H_
