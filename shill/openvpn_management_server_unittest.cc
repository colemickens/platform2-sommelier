// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_management_server.h"

#include <gtest/gtest.h>

#include "shill/key_value_store.h"
#include "shill/mock_openvpn_driver.h"

namespace shill {

class OpenVPNManagementServerTest : public testing::Test {
 public:
  OpenVPNManagementServerTest()
      : driver_(args_),
        server_(&driver_) {}

  virtual ~OpenVPNManagementServerTest() {}

 protected:
  KeyValueStore args_;
  MockOpenVPNDriver driver_;
  OpenVPNManagementServer server_;
};

TEST_F(OpenVPNManagementServerTest, Start) {
  // TODO(petkov): Add unit tests.
}

}  // namespace shill
