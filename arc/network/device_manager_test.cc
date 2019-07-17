// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include <base/message_loop/message_loop.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/mock_bus.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "arc/network/fake_shill_client.h"

namespace arc_networkd {

namespace {

class FakeAddressManager : public AddressManager {
 public:
  FakeAddressManager()
      : AddressManager({
            AddressManager::Guest::ARC,
            AddressManager::Guest::ARC_NET,
        }) {}

  MacAddress GenerateMacAddress() override {
    static const MacAddress kAddr = {0xf7, 0x69, 0xe5, 0xc4, 0x1f, 0x74};
    return kAddr;
  }
};

class DeviceManagerTest : public testing::Test {
 protected:
  DeviceManagerTest() : testing::Test() {}

  void SetUp() override {
    capture_msgs_ = false;

    android_announce_msg_.set_dev_ifname(kAndroidDevice);
    auto* config = android_announce_msg_.mutable_dev_config();
    config->set_br_ifname("arcbr0");
    config->set_br_ipv4("100.115.92.1");
    config->set_arc_ifname("arc0");
    config->set_arc_ipv4("100.115.92.2");
    config->set_mac_addr("f7:69:e5:c4:1f:74");
    config->set_fwd_multicast(false);
    config->set_find_ipv6_routes(false);

    legacy_android_announce_msg_.set_dev_ifname(kAndroidLegacyDevice);
    config = legacy_android_announce_msg_.mutable_dev_config();
    config->set_br_ifname("arcbr0");
    config->set_br_ipv4("100.115.92.1");
    config->set_arc_ifname("arc0");
    config->set_arc_ipv4("100.115.92.2");
    config->set_mac_addr("f7:69:e5:c4:1f:74");
    config->set_fwd_multicast(true);
    config->set_find_ipv6_routes(true);
  }

  std::unique_ptr<DeviceManager> NewManager(bool is_arc_legacy = false) {
    shill_helper_ = std::make_unique<FakeShillClientHelper>();
    auto shill_client = shill_helper_->FakeClient();
    shill_client_ = shill_client.get();
    auto mgr = std::make_unique<DeviceManager>(
        std::move(shill_client), &addr_mgr_,
        base::Bind(&DeviceManagerTest::RecvMsg, base::Unretained(this)),
        is_arc_legacy);
    mgr->OnGuestStart();
    return mgr;
  }

  void VerifyMsgs(const std::vector<DeviceMessage>& expected) {
    ASSERT_EQ(msgs_recv_.size(), expected.size());
    for (size_t i = 0; i < msgs_recv_.size(); ++i) {
      EXPECT_EQ(msgs_recv_[i], expected[i].SerializeAsString());
    }
  }

  void ClearMsgs() { msgs_recv_.clear(); }

  bool capture_msgs_;
  FakeShillClient* shill_client_;
  DeviceMessage android_announce_msg_;
  DeviceMessage legacy_android_announce_msg_;

 private:
  void RecvMsg(const DeviceMessage& msg) {
    if (capture_msgs_)
      msgs_recv_.emplace_back(msg.SerializeAsString());
  }

  std::unique_ptr<FakeShillClientHelper> shill_helper_;

  std::vector<std::string> msgs_recv_;
  FakeAddressManager addr_mgr_;

  // This is required because of the RT netlink handler.
  base::MessageLoopForIO msg_loop_;
};

}  // namespace

TEST_F(DeviceManagerTest, MakeEthernetDevices) {
  auto mgr = NewManager();
  DeviceConfig msg;

  auto eth_device0 = mgr->MakeDevice("eth0");
  eth_device0->FillProto(&msg);
  EXPECT_EQ(msg.br_ifname(), "arc_eth0");
  EXPECT_EQ(msg.arc_ifname(), "eth0");
  EXPECT_EQ(msg.br_ipv4(), "100.115.92.9");
  EXPECT_EQ(msg.arc_ipv4(), "100.115.92.10");
  EXPECT_FALSE(msg.mac_addr().empty());
  EXPECT_TRUE(msg.fwd_multicast());
  EXPECT_TRUE(msg.find_ipv6_routes());

  auto eth_device1 = mgr->MakeDevice("usb0");
  eth_device1->FillProto(&msg);
  EXPECT_EQ(msg.br_ifname(), "arc_usb0");
  EXPECT_EQ(msg.arc_ifname(), "usb0");
  EXPECT_EQ(msg.br_ipv4(), "100.115.92.13");
  EXPECT_EQ(msg.arc_ipv4(), "100.115.92.14");
  EXPECT_FALSE(msg.mac_addr().empty());
  EXPECT_TRUE(msg.fwd_multicast());
  EXPECT_TRUE(msg.find_ipv6_routes());
}

TEST_F(DeviceManagerTest, MakeWifiDevices) {
  auto mgr = NewManager();
  DeviceConfig msg;

  auto wifi_device0 = mgr->MakeDevice("wlan0");
  wifi_device0->FillProto(&msg);
  EXPECT_EQ(msg.br_ifname(), "arc_wlan0");
  EXPECT_EQ(msg.arc_ifname(), "wlan0");
  EXPECT_EQ(msg.br_ipv4(), "100.115.92.9");
  EXPECT_EQ(msg.arc_ipv4(), "100.115.92.10");
  EXPECT_FALSE(msg.mac_addr().empty());
  EXPECT_TRUE(msg.fwd_multicast());
  EXPECT_TRUE(msg.find_ipv6_routes());

  auto wifi_device1 = mgr->MakeDevice("mlan0");
  wifi_device1->FillProto(&msg);
  EXPECT_EQ(msg.br_ifname(), "arc_mlan0");
  EXPECT_EQ(msg.arc_ifname(), "mlan0");
  EXPECT_EQ(msg.br_ipv4(), "100.115.92.13");
  EXPECT_EQ(msg.arc_ipv4(), "100.115.92.14");
  EXPECT_FALSE(msg.mac_addr().empty());
  EXPECT_TRUE(msg.fwd_multicast());
  EXPECT_TRUE(msg.find_ipv6_routes());
}

TEST_F(DeviceManagerTest, MakeCellularDevice) {
  auto mgr = NewManager();
  DeviceConfig msg;
  mgr->MakeDevice("wwan0")->FillProto(&msg);
  EXPECT_EQ(msg.br_ifname(), "arc_wwan0");
  EXPECT_EQ(msg.arc_ifname(), "wwan0");
  EXPECT_EQ(msg.br_ipv4(), "100.115.92.9");
  EXPECT_EQ(msg.arc_ipv4(), "100.115.92.10");
  EXPECT_FALSE(msg.mac_addr().empty());
  EXPECT_FALSE(msg.fwd_multicast());
  EXPECT_FALSE(msg.find_ipv6_routes());
}

TEST_F(DeviceManagerTest, MakeDevice_Android) {
  capture_msgs_ = true;
  auto mgr = NewManager();
  VerifyMsgs({android_announce_msg_});

  // Can't add another.
  EXPECT_FALSE(mgr->MakeDevice(kAndroidDevice));
}

TEST_F(DeviceManagerTest, MakeDevice_LegacyAndroid) {
  capture_msgs_ = true;
  auto mgr = NewManager(false);
  VerifyMsgs({android_announce_msg_});

  // Can't add another.
  EXPECT_FALSE(mgr->MakeDevice(kAndroidLegacyDevice));
}

TEST_F(DeviceManagerTest, MakeVpnTunDevice) {
  auto mgr = NewManager();
  DeviceConfig msg;
  mgr->MakeDevice("tun0")->FillProto(&msg);
  EXPECT_EQ(msg.br_ifname(), "arc_tun0");
  EXPECT_EQ(msg.arc_ifname(), "cros_tun0");
  EXPECT_EQ(msg.br_ipv4(), "100.115.92.9");
  EXPECT_EQ(msg.arc_ipv4(), "100.115.92.10");
  EXPECT_FALSE(msg.mac_addr().empty());
  EXPECT_FALSE(msg.fwd_multicast());
  EXPECT_FALSE(msg.find_ipv6_routes());
}

TEST_F(DeviceManagerTest, MakeDevice_NoMoreSubnets) {
  auto mgr = NewManager();
  DeviceConfig msg;
  std::vector<std::unique_ptr<Device>> devices;
  for (int i = 0; i < 4; ++i) {
    auto dev = mgr->MakeDevice(base::StringPrintf("%d", i));
    EXPECT_TRUE(dev);
    devices.emplace_back(std::move(dev));
  }
  EXPECT_FALSE(mgr->MakeDevice("x"));
}

TEST_F(DeviceManagerTest, AndroidDeviceAddedByDefault) {
  auto mgr = NewManager();
  EXPECT_TRUE(mgr->Exists(kAndroidDevice));
  EXPECT_FALSE(mgr->Exists(kAndroidLegacyDevice));
}

TEST_F(DeviceManagerTest,
       AndroidLegacyDeviceAddedByDefaultWhenMultinetDisabled) {
  auto mgr = NewManager(true /* is_arc_legacy */);
  EXPECT_FALSE(mgr->Exists(kAndroidDevice));
  EXPECT_TRUE(mgr->Exists(kAndroidLegacyDevice));
}

TEST_F(DeviceManagerTest, AddNewDevices) {
  auto mgr = NewManager();
  std::vector<dbus::ObjectPath> devices = {dbus::ObjectPath("eth0"),
                                           dbus::ObjectPath("wlan0")};
  auto value = brillo::Any(devices);
  shill_client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
  EXPECT_TRUE(mgr->Exists("eth0"));
  EXPECT_TRUE(mgr->Exists("wlan0"));
  // Verify Android device is not removed.
  EXPECT_TRUE(mgr->Exists(kAndroidDevice));
}

TEST_F(DeviceManagerTest, NoDevicesAddedWhenMultinetDisabled) {
  auto mgr = NewManager(true /* is_arc_legacy */);
  std::vector<dbus::ObjectPath> devices = {dbus::ObjectPath("eth0"),
                                           dbus::ObjectPath("wlan0")};
  auto value = brillo::Any(devices);
  shill_client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
  EXPECT_FALSE(mgr->Exists("eth0"));
  EXPECT_FALSE(mgr->Exists("wlan0"));
  // Verify Android device is not removed.
  EXPECT_TRUE(mgr->Exists(kAndroidLegacyDevice));
}

TEST_F(DeviceManagerTest, PreviousDevicesRemoved) {
  auto mgr = NewManager();
  {
    std::vector<dbus::ObjectPath> devices = {dbus::ObjectPath("eth0"),
                                             dbus::ObjectPath("wlan0")};
    auto value = brillo::Any(devices);
    shill_client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
    EXPECT_TRUE(mgr->Exists("eth0"));
    EXPECT_TRUE(mgr->Exists("wlan0"));
  }
  {
    std::vector<dbus::ObjectPath> devices = {dbus::ObjectPath("eth0"),
                                             dbus::ObjectPath("eth1")};
    auto value = brillo::Any(devices);
    shill_client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
    EXPECT_TRUE(mgr->Exists("eth0"));
    EXPECT_TRUE(mgr->Exists("eth1"));
    EXPECT_FALSE(mgr->Exists("wlan0"));
  }
}

}  // namespace arc_networkd
