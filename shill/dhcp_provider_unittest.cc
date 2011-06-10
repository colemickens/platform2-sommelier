// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_provider.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_glib.h"

using testing::Test;

namespace shill {

namespace {
const char kDeviceName[] = "testdevicename";
}  // namespace {}

class DHCPProviderTest : public Test {
 public:
  DHCPProviderTest()
      : device_(new MockDevice(&control_interface_,
                               NULL,
                               NULL,
                               kDeviceName,
                               0)),
        provider_(DHCPProvider::GetInstance()) {
    provider_->glib_ = &glib_;
  }

 protected:
  MockGLib glib_;
  MockControl control_interface_;
  scoped_refptr<MockDevice> device_;
  DHCPProvider *provider_;
};

TEST_F(DHCPProviderTest, CreateConfig) {
  DHCPConfigRefPtr config = provider_->CreateConfig(device_);
  EXPECT_TRUE(config.get());
  EXPECT_EQ(&glib_, config->glib_);
  EXPECT_EQ(kDeviceName, config->GetDeviceName());
  EXPECT_TRUE(provider_->configs_.empty());
}

}  // namespace shill
