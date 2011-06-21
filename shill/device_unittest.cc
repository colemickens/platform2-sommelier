// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "shill/device.h"
#include "shill/dhcp_provider.h"
#include "shill/mock_control.h"
#include "shill/mock_glib.h"

using testing::_;
using testing::Return;
using testing::Test;

namespace shill {

namespace {
const char kDeviceName[] = "testdevice";
}  // namespace {}

class DeviceTest : public Test {
 public:
  DeviceTest()
      : device_(new Device(&control_interface_, NULL, NULL, kDeviceName, 0)) {
    DHCPProvider::GetInstance()->glib_ = &glib_;
  }

 protected:
  MockGLib glib_;
  MockControl control_interface_;
  DeviceRefPtr device_;
};

TEST_F(DeviceTest, TechnologyIs) {
  EXPECT_FALSE(device_->TechnologyIs(Device::kEthernet));
}

TEST_F(DeviceTest, DestroyIPConfig) {
  ASSERT_FALSE(device_->ipconfig_.get());
  device_->ipconfig_ = new IPConfig(kDeviceName);
  device_->DestroyIPConfig();
  ASSERT_FALSE(device_->ipconfig_.get());
}

TEST_F(DeviceTest, DestroyIPConfigNULL) {
  ASSERT_FALSE(device_->ipconfig_.get());
  device_->DestroyIPConfig();
  ASSERT_FALSE(device_->ipconfig_.get());
}

TEST_F(DeviceTest, AcquireDHCPConfig) {
  device_->ipconfig_ = new IPConfig("randomname");
  EXPECT_CALL(glib_, SpawnAsync(_, _, _, _, _, _, _, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(device_->AcquireDHCPConfig());
  ASSERT_TRUE(device_->ipconfig_.get());
  EXPECT_EQ(kDeviceName, device_->ipconfig_->device_name());
  EXPECT_TRUE(device_->ipconfig_->update_callback_.get());
}

}  // namespace shill
