// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device.h"

#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "arc/network/mac_address_generator.h"
#include "arc/network/subnet.h"

namespace arc_networkd {

namespace {

void DoNothing() {}

class DeviceTest : public testing::Test {
 protected:
  void SetUp() override { capture_msgs_ = false; }

  std::unique_ptr<Device> NewDevice(const std::string& name,
                                    const Device::Options& options) {
    auto ipv4_subnet =
        std::make_unique<Subnet>(0x64646464, 30, base::Bind(&DoNothing));
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

  void VerifyMsgs(const std::vector<IpHelperMessage>& expected) {
    ASSERT_EQ(msgs_recv_.size(), expected.size());
    for (int i = 0; i < msgs_recv_.size(); ++i) {
      EXPECT_EQ(msgs_recv_[i], expected[i].SerializeAsString());
    }
  }

  bool capture_msgs_;
  DeviceConfig msg_;

 private:
  void RecvMsg(const IpHelperMessage& msg) {
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

TEST_F(DeviceTest, CtorSendsAnnounce) {
  capture_msgs_ = true;
  Device::Options opts = {true, true};
  auto dev = NewDevice(kAndroidDevice, opts);
  IpHelperMessage msg;
  msg.set_dev_ifname(kAndroidDevice);
  *msg.mutable_dev_config() = msg_;
  VerifyMsgs({msg});
}

TEST_F(DeviceTest, DtorSendsTeardown) {
  Device::Options opts = {true, true};
  auto dev = NewDevice(kAndroidDevice, opts);
  capture_msgs_ = true;
  dev.reset();
  IpHelperMessage msg;
  msg.set_dev_ifname(kAndroidDevice);
  msg.set_teardown(true);
  VerifyMsgs({msg});
}

TEST_F(DeviceTest, EnableSendsMessageForLegacyAndroid) {
  Device::Options opts = {false, false};
  auto dev = NewDevice(kAndroidLegacyDevice, opts);
  capture_msgs_ = true;
  dev->Enable("eth0");
  IpHelperMessage enable_msg;
  enable_msg.set_dev_ifname(kAndroidLegacyDevice);
  enable_msg.set_enable_inbound_ifname("eth0");
  VerifyMsgs({enable_msg});
}

TEST_F(DeviceTest, EnableDoesNothingForNonLegacyAndroid) {
  Device::Options opts = {false, false};
  auto dev = NewDevice(kAndroidDevice, opts);
  capture_msgs_ = true;
  dev->Enable("eth0");
  VerifyMsgs({});
}

TEST_F(DeviceTest, DisableLegacyAndroidDeviceSendsTwoMessages) {
  Device::Options opts = {false, false};
  auto dev = NewDevice(kAndroidLegacyDevice, opts);
  dev->Enable("eth0");
  capture_msgs_ = true;
  // HACK(garrick): We have to turn off IPv6 route finding during testing
  // to avoid an unrelated crash but the Android device does have IPv6
  // route finding enabled, so we want to verify the 'clear' message is
  // emitted for this device. This hack allows the check to pass and the
  // message to be sent.
  const_cast<Device::Options*>(&dev->options_)->find_ipv6_routes = true;
  dev->Disable();
  IpHelperMessage clear_msg;
  clear_msg.set_dev_ifname(kAndroidLegacyDevice);
  clear_msg.set_clear_arc_ip(true);
  IpHelperMessage disable_msg;
  disable_msg.set_dev_ifname(kAndroidLegacyDevice);
  disable_msg.set_disable_inbound(true);
  VerifyMsgs({clear_msg, disable_msg});
}

TEST_F(DeviceTest, DisableDoesNothingForNonLegacyAndroid) {
  Device::Options opts = {false, false};
  auto dev = NewDevice(kAndroidDevice, opts);
  capture_msgs_ = true;
  dev->Disable();
  VerifyMsgs({});
}

TEST_F(DeviceTest, DisableDoesNothingIfNotEnabled) {
  Device::Options opts = {false, false};
  auto dev = NewDevice(kAndroidLegacyDevice, opts);
  capture_msgs_ = true;
  dev->Disable();
  VerifyMsgs({});
}

TEST_F(DeviceTest, ClearMessageNotSentIfIPv6RouteFindingIsOff) {
  Device::Options opts = {false, false};
  auto dev = NewDevice(kAndroidLegacyDevice, opts);
  dev->Enable("eth0");
  capture_msgs_ = true;
  dev->Disable();
  IpHelperMessage disable_msg;
  disable_msg.set_dev_ifname(kAndroidLegacyDevice);
  disable_msg.set_disable_inbound(true);
  VerifyMsgs({disable_msg});
}

}  // namespace arc_networkd
