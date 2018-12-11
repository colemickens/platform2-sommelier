// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device.h"

#include <vector>

#include <gtest/gtest.h>

namespace arc_networkd {

namespace {

class DeviceTest : public testing::Test {
 protected:
  void SetUp() override { capture_msgs_ = false; }

  std::unique_ptr<Device> NewDevice(const std::string& name) {
    return Device::ForInterface(
        name, base::Bind(&DeviceTest::RecvMsg, base::Unretained(this)));
  }

  std::unique_ptr<Device> NewDevice(const std::string& name,
                                    bool fwd_multicast,
                                    bool find_ipv6_routes) {
    DeviceConfig config;
    config.set_br_ifname(name);
    config.set_arc_ifname(name);
    config.set_br_ipv4("1.2.3.4");
    config.set_arc_ipv4("1.2.3.4");
    config.set_mac_addr("mac");
    config.set_fwd_multicast(fwd_multicast);
    config.set_find_ipv6_routes(find_ipv6_routes);
    return std::make_unique<Device>(
        name, config, base::Bind(&DeviceTest::RecvMsg, base::Unretained(this)));
  }

  void VerifyMsgs(const std::vector<IpHelperMessage>& expected) {
    ASSERT_EQ(msgs_recv_.size(), expected.size());
    for (int i = 0; i < msgs_recv_.size(); ++i) {
      EXPECT_EQ(msgs_recv_[i], expected[i].SerializeAsString());
    }
  }

  bool capture_msgs_;

 private:
  void RecvMsg(const IpHelperMessage& msg) {
    if (capture_msgs_)
      msgs_recv_.emplace_back(msg.SerializeAsString());
  }

  std::vector<std::string> msgs_recv_;
};

}  // namespace

TEST_F(DeviceTest, ConfigForAndroid) {
  auto dev = NewDevice(kAndroidDevice);
  ASSERT_NE(dev, nullptr);
  auto& config = dev->config();
  EXPECT_EQ(config.br_ifname(), "arcbr0");
  EXPECT_EQ(config.br_ipv4(), "100.115.92.1");
  EXPECT_EQ(config.arc_ifname(), "arc0");
  EXPECT_EQ(config.arc_ipv4(), "100.115.92.2");
  EXPECT_EQ(config.mac_addr(), "00:FF:AA:00:00:56");
  EXPECT_TRUE(config.fwd_multicast());
  EXPECT_TRUE(config.find_ipv6_routes());
}

TEST_F(DeviceTest, ConfigForEth0) {
  auto dev = NewDevice("eth0");
  ASSERT_NE(dev, nullptr);
  auto& config = dev->config();
  EXPECT_EQ(config.br_ifname(), "arc_eth0");
  EXPECT_EQ(config.br_ipv4(), "100.115.92.5");
  EXPECT_EQ(config.arc_ifname(), "eth0");
  EXPECT_EQ(config.arc_ipv4(), "100.115.92.6");
  EXPECT_EQ(config.mac_addr(), "00:FF:AA:00:00:5a");
  EXPECT_TRUE(config.fwd_multicast());
  EXPECT_FALSE(config.find_ipv6_routes());
}

TEST_F(DeviceTest, ConfigForWlan0) {
  auto dev = NewDevice("wlan0");
  ASSERT_NE(dev, nullptr);
  auto& config = dev->config();
  EXPECT_EQ(config.br_ifname(), "arc_wlan0");
  EXPECT_EQ(config.br_ipv4(), "100.115.92.9");
  EXPECT_EQ(config.arc_ifname(), "wlan0");
  EXPECT_EQ(config.arc_ipv4(), "100.115.92.10");
  EXPECT_EQ(config.mac_addr(), "00:FF:AA:00:00:5e");
  EXPECT_TRUE(config.fwd_multicast());
  EXPECT_FALSE(config.find_ipv6_routes());
}

TEST_F(DeviceTest, ConfigForUnknown) {
  EXPECT_EQ(NewDevice(""), nullptr);
  EXPECT_EQ(NewDevice("unk"), nullptr);
  EXPECT_EQ(NewDevice("eth1"), nullptr);
  EXPECT_EQ(NewDevice("wlan1"), nullptr);
}

TEST_F(DeviceTest, CtorSendsAnnounce) {
  capture_msgs_ = true;
  auto dev = NewDevice(kAndroidDevice);
  IpHelperMessage msg;
  msg.set_dev_ifname(kAndroidDevice);
  *msg.mutable_dev_config() = dev->config();
  VerifyMsgs({msg});
}

TEST_F(DeviceTest, DtorSendsTeardown) {
  auto dev = NewDevice(kAndroidDevice);
  capture_msgs_ = true;
  dev.reset();
  IpHelperMessage msg;
  msg.set_dev_ifname(kAndroidDevice);
  msg.set_teardown(true);
  VerifyMsgs({msg});
}

TEST_F(DeviceTest, EnableSendsMessage) {
  auto dev = NewDevice(kAndroidDevice, false, false);
  capture_msgs_ = true;
  dev->Enable("eth0");
  IpHelperMessage enable_msg;
  enable_msg.set_dev_ifname(kAndroidDevice);
  enable_msg.set_enable_inbound_ifname("eth0");
  VerifyMsgs({enable_msg});
}

TEST_F(DeviceTest, DisableAndroidDeviceSendsTwoMessages) {
  auto dev = NewDevice(kAndroidDevice, false, false);
  capture_msgs_ = true;
  // HACK(garrick): We have to turn off IPv6 route finding during testing
  // to avoid an unrelated crash but the Android device does have IPv6
  // route finding enabled, so we want to verify the 'clear' message is
  // emitted for this device. This hack allows the check to pass and the
  // message to be sent.
  const_cast<DeviceConfig*>(&dev->config())->set_find_ipv6_routes(true);
  dev->Disable();
  IpHelperMessage clear_msg;
  clear_msg.set_dev_ifname(kAndroidDevice);
  clear_msg.set_clear_arc_ip(true);
  IpHelperMessage disable_msg;
  disable_msg.set_dev_ifname(kAndroidDevice);
  disable_msg.set_disable_inbound(true);
  VerifyMsgs({clear_msg, disable_msg});
}

TEST_F(DeviceTest, ClearMessageNotSentIfIPv6RouteFindingIsOff) {
  auto dev = NewDevice(kAndroidDevice, false, false);
  capture_msgs_ = true;
  dev->Disable();
  IpHelperMessage disable_msg;
  disable_msg.set_dev_ifname(kAndroidDevice);
  disable_msg.set_disable_inbound(true);
  VerifyMsgs({disable_msg});
}

}  // namespace arc_networkd
