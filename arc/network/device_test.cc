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
 protected:
  void SetUp() override { capture_msgs_ = false; }

  std::unique_ptr<Device> NewDevice(const std::string& name,
                                    const Device::Options& options) {
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
    auto dev = std::make_unique<Device>(
        name, std::move(config), options,
        base::Bind(&DeviceTest::RecvMsg, base::Unretained(this)));

    dev->FillProto(&msg_);
    return dev;
  }

  void VerifyMsgs(const std::vector<DeviceMessage>& expected) {
    ASSERT_EQ(msgs_recv_.size(), expected.size());
    for (int i = 0; i < msgs_recv_.size(); ++i) {
      EXPECT_EQ(msgs_recv_[i], expected[i].SerializeAsString());
    }
  }

  bool capture_msgs_;
  DeviceConfig msg_;

 private:
  void RecvMsg(const DeviceMessage& msg) {
    if (capture_msgs_)
      msgs_recv_.emplace_back(msg.SerializeAsString());
  }

  std::vector<std::string> msgs_recv_;
};

}  // namespace

TEST_F(DeviceTest, FillProto) {
  Device::Options opts = {true, true};
  auto dev = NewDevice(kAndroidDevice, opts);
  DeviceConfig msg;
  dev->FillProto(&msg);
  EXPECT_EQ(msg.br_ifname(), "host");
  EXPECT_EQ(msg.arc_ifname(), "guest");
  EXPECT_EQ(msg.br_ipv4(), "100.100.100.101");
  EXPECT_EQ(msg.arc_ipv4(), "100.100.100.102");
  EXPECT_FALSE(msg.mac_addr().empty());
  EXPECT_TRUE(msg.fwd_multicast());
  EXPECT_TRUE(msg.find_ipv6_routes());
}

}  // namespace arc_networkd
