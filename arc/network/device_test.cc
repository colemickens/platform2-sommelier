// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device.h"

#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "arc/network/mac_address_generator.h"
#include "arc/network/net_util.h"
#include "arc/network/subnet.h"

namespace arc_networkd {

namespace {

void DoNothing() {}

class DeviceTest : public testing::Test {
 public:
  void IPv6Down(Device* device) { ipv6_down_ = true; }

 protected:
  void SetUp() override { ipv6_down_ = false; }

  std::unique_ptr<Device> NewDevice(const std::string& name) {
    Device::Options options{
        .ipv6_enabled = true,
        .find_ipv6_routes_legacy = true,
        .use_default_interface = (name == kAndroidLegacyDevice),
        .is_android = (name == kAndroidDevice || name == kAndroidLegacyDevice),
    };

    auto ipv4_subnet = std::make_unique<Subnet>(Ipv4Addr(100, 100, 100, 100),
                                                30, base::Bind(&DoNothing));
    EXPECT_TRUE(ipv4_subnet);

    auto host_ipv4_addr = ipv4_subnet->AllocateAtOffset(0);
    EXPECT_TRUE(host_ipv4_addr);

    auto guest_ipv4_addr = ipv4_subnet->AllocateAtOffset(1);
    EXPECT_TRUE(guest_ipv4_addr);

    auto config = std::make_unique<Device::Config>(
        "host", "guest", MacAddressGenerator().Generate(),
        std::move(ipv4_subnet), std::move(host_ipv4_addr),
        std::move(guest_ipv4_addr));
    return std::make_unique<Device>(name, std::move(config), options);
  }

  bool ipv6_down_;
};

TEST_F(DeviceTest, IsAndroid) {
  auto dev = NewDevice(kAndroidDevice);
  EXPECT_TRUE(dev->IsAndroid());
  EXPECT_FALSE(dev->UsesDefaultInterface());
  dev = NewDevice(kAndroidLegacyDevice);
  EXPECT_TRUE(dev->IsAndroid());
  EXPECT_TRUE(dev->UsesDefaultInterface());
  dev = NewDevice("eth0");
  EXPECT_FALSE(dev->IsAndroid());
  EXPECT_FALSE(dev->UsesDefaultInterface());
}

TEST_F(DeviceTest, IPv6TeardownHandlerCalledOnDisable) {
  auto dev = NewDevice("foo");
  dev->RegisterIPv6TeardownHandler(
      base::Bind(&DeviceTest::IPv6Down, base::Unretained(this)));
  dev->Disable();
  EXPECT_TRUE(ipv6_down_);
}

}  // namespace

}  // namespace arc_networkd
