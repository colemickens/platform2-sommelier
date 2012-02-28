// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_driver.h"

#include <gtest/gtest.h>

#include "shill/error.h"

namespace shill {

class OpenVPNDriverTest : public testing::Test {
 public:
  OpenVPNDriverTest() {}
  virtual ~OpenVPNDriverTest() {}

 protected:
  OpenVPNDriver driver_;
};

TEST_F(OpenVPNDriverTest, Connect) {
  Error error;
  driver_.Connect(&error);
  EXPECT_EQ(Error::kNotSupported, error.type());
}

}  // namespace shill
