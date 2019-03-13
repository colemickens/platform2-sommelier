// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device_manager.h"

#include <utility>
#include <vector>

#include <base/strings/stringprintf.h>
#include <gtest/gtest.h>

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
    NewManager("")
        ->MakeDevice(kAndroidDevice)
        ->FillProto(android_announce_msg_.mutable_dev_config());

    legacy_android_announce_msg_.set_dev_ifname(kAndroidLegacyDevice);
    NewManager("")
        ->MakeDevice(kAndroidLegacyDevice)
        ->FillProto(legacy_android_announce_msg_.mutable_dev_config());
  }

  std::unique_ptr<DeviceManager> NewManager(
      const std::string& arc_device = kAndroidDevice) {
    return std::make_unique<DeviceManager>(
        &addr_mgr_,
        base::Bind(&DeviceManagerTest::RecvMsg, base::Unretained(this)),
        arc_device);
  }

  void VerifyMsgs(const std::vector<IpHelperMessage>& expected) {
    ASSERT_EQ(msgs_recv_.size(), expected.size());
    for (size_t i = 0; i < msgs_recv_.size(); ++i) {
      EXPECT_EQ(msgs_recv_[i], expected[i].SerializeAsString());
    }
  }

  void ClearMsgs() { msgs_recv_.clear(); }

  bool capture_msgs_;
  IpHelperMessage android_announce_msg_;
  IpHelperMessage legacy_android_announce_msg_;

 private:
  void RecvMsg(const IpHelperMessage& msg) {
    if (capture_msgs_)
      msgs_recv_.emplace_back(msg.SerializeAsString());
  }

  std::vector<std::string> msgs_recv_;
  FakeAddressManager addr_mgr_;
};

}  // namespace

TEST_F(DeviceManagerTest, MakeDevice) {
  auto mgr = NewManager();
  DeviceConfig msg;
  mgr->MakeDevice("eth0")->FillProto(&msg);
  EXPECT_EQ(msg.br_ifname(), "arc_eth0");
  EXPECT_EQ(msg.arc_ifname(), "eth0");
  EXPECT_EQ(msg.br_ipv4(), "100.115.92.9");
  EXPECT_EQ(msg.arc_ipv4(), "100.115.92.10");
  EXPECT_FALSE(msg.mac_addr().empty());
  EXPECT_FALSE(msg.fwd_multicast());
  EXPECT_FALSE(msg.find_ipv6_routes());
}

TEST_F(DeviceManagerTest, MakeDevice_Android) {
  auto mgr = NewManager("" /* no default arc device */);
  DeviceConfig msg;
  mgr->MakeDevice(kAndroidDevice)->FillProto(&msg);
  EXPECT_EQ(msg.br_ifname(), "arcbr0");
  EXPECT_EQ(msg.arc_ifname(), "arc0");
  EXPECT_EQ(msg.br_ipv4(), "100.115.92.1");
  EXPECT_EQ(msg.arc_ipv4(), "100.115.92.2");
  EXPECT_FALSE(msg.mac_addr().empty());
  EXPECT_FALSE(msg.fwd_multicast());
  EXPECT_FALSE(msg.find_ipv6_routes());
}

TEST_F(DeviceManagerTest, MakeDevice_LegacyAndroid) {
  auto mgr = NewManager("" /* no default arc device */);
  DeviceConfig msg;
  mgr->MakeDevice(kAndroidLegacyDevice)->FillProto(&msg);
  EXPECT_EQ(msg.br_ifname(), "arcbr0");
  EXPECT_EQ(msg.arc_ifname(), "arc0");
  EXPECT_EQ(msg.br_ipv4(), "100.115.92.1");
  EXPECT_EQ(msg.arc_ipv4(), "100.115.92.2");
  EXPECT_FALSE(msg.mac_addr().empty());
  EXPECT_TRUE(msg.fwd_multicast());
  EXPECT_TRUE(msg.find_ipv6_routes());
}

TEST_F(DeviceManagerTest, MakeDevice_NoMoreSubnets) {
  auto mgr = NewManager("" /* no default arc device */);
  DeviceConfig msg;
  {
    auto dev = mgr->MakeDevice(kAndroidDevice);
    EXPECT_TRUE(dev);
    EXPECT_FALSE(mgr->MakeDevice(kAndroidDevice));
  }
  {
    auto dev = mgr->MakeDevice(kAndroidLegacyDevice);
    EXPECT_TRUE(dev);
    EXPECT_FALSE(mgr->MakeDevice(kAndroidLegacyDevice));
  }
  {
    std::vector<std::unique_ptr<Device>> devices;
    for (int i = 0; i < 4; ++i) {
      auto dev = mgr->MakeDevice(base::StringPrintf("%d", i));
      EXPECT_TRUE(dev);
      devices.emplace_back(std::move(dev));
    }
    EXPECT_FALSE(mgr->MakeDevice("x"));
  }
}

TEST_F(DeviceManagerTest, CtorAddsAndroidDevice) {
  capture_msgs_ = true;
  auto mgr = NewManager();
  VerifyMsgs({android_announce_msg_});
}

TEST_F(DeviceManagerTest, CtorAddsLegacyAndroidDevice_MultinetDisabled) {
  capture_msgs_ = true;
  auto mgr = NewManager(kAndroidLegacyDevice);
  VerifyMsgs({legacy_android_announce_msg_});
}

TEST_F(DeviceManagerTest, AddNewDevices) {
  auto mgr = NewManager();
  EXPECT_TRUE(mgr->Add("eth0"));
}

TEST_F(DeviceManagerTest, CannotAddExistingDevice) {
  auto mgr = NewManager();
  EXPECT_FALSE(mgr->Add(kAndroidDevice));
}

TEST_F(DeviceManagerTest, CannotAddExistingDevice_MultinetDisabled) {
  auto mgr = NewManager(kAndroidLegacyDevice);
  EXPECT_FALSE(mgr->Add(kAndroidLegacyDevice));
}

TEST_F(DeviceManagerTest, ResetAddsNewDevices) {
  auto mgr = NewManager();
  EXPECT_TRUE(mgr->Add("eth0"));
  EXPECT_EQ(mgr->Reset({"eth0", "wlan0"}), 3);
}

TEST_F(DeviceManagerTest, ResetRemovesExistingDevices) {
  auto mgr = NewManager();
  EXPECT_TRUE(mgr->Add("eth0"));
  EXPECT_EQ(mgr->Reset({"wlan0"}), 2);
  // Can use Add now to verify only android and wlan0 exist (eth0 removed).
  EXPECT_FALSE(mgr->Add(kAndroidDevice));
  EXPECT_FALSE(mgr->Add("wlan0"));
}

TEST_F(DeviceManagerTest, EnableDoesNothing_Multinet) {
  auto mgr = NewManager(kAndroidDevice);
  EXPECT_FALSE(mgr->Enable("eth0"));
}

TEST_F(DeviceManagerTest, DisableDoesNothing_Multinet) {
  auto mgr = NewManager(kAndroidDevice);
  EXPECT_FALSE(mgr->Enable("eth0"));
  EXPECT_FALSE(mgr->Disable());
}

TEST_F(DeviceManagerTest, Enable_MultinetDisabled) {
  auto mgr = NewManager(kAndroidLegacyDevice);
  // Need to use an empty interface here so that Device::Enable does not
  // proceed since it will crash during testing while trying to set up
  // IPv6 route finding.
  EXPECT_TRUE(mgr->Enable(""));
}

TEST_F(DeviceManagerTest, CanDisableLegacyAndroidDevice_MultinetDisabled) {
  auto mgr = NewManager(kAndroidLegacyDevice);
  EXPECT_TRUE(mgr->Enable(""));
  EXPECT_TRUE(mgr->Disable());
}

}  // namespace arc_networkd
