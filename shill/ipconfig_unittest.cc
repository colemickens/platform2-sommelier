// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ipconfig.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"

namespace shill {
using ::testing::Test;

class IPConfigTest : public Test {
 public:
  IPConfigTest() :
      device_(new MockDevice(&control_interface_, NULL, "testname", 0)) {}

 protected:
  MockControl control_interface_;
  scoped_refptr<MockDevice> device_;
};

TEST_F(IPConfigTest, GetDeviceNameTest) {
  scoped_refptr<IPConfig> ipconfig(new IPConfig(device_.get()));
  EXPECT_EQ("testname", ipconfig->GetDeviceName());
}

}  // namespace shill
