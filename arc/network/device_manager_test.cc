// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/device_manager.h"

#include <vector>

#include <gtest/gtest.h>

namespace arc_networkd {

namespace {

class DeviceManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    capture_msgs_ = false;
    enable_multinet_ = true;

    auto dev = Device::ForInterface(
        kAndroidLegacyDevice,
        base::Bind(&DeviceManagerTest::RecvMsg, base::Unretained(this)));
    legacy_android_announce_msg_.set_dev_ifname(kAndroidLegacyDevice);
    *legacy_android_announce_msg_.mutable_dev_config() = dev->config();

    dev = Device::ForInterface(
        kAndroidDevice,
        base::Bind(&DeviceManagerTest::RecvMsg, base::Unretained(this)));
    android_announce_msg_.set_dev_ifname(kAndroidDevice);
    *android_announce_msg_.mutable_dev_config() = dev->config();
  }

  std::unique_ptr<DeviceManager> NewManager() {
    return std::make_unique<DeviceManager>(
        base::Bind(&DeviceManagerTest::RecvMsg, base::Unretained(this)),
        enable_multinet_ ? kAndroidDevice : kAndroidLegacyDevice);
  }

  void VerifyMsgs(const std::vector<IpHelperMessage>& expected) {
    ASSERT_EQ(msgs_recv_.size(), expected.size());
    for (size_t i = 0; i < msgs_recv_.size(); ++i) {
      EXPECT_EQ(msgs_recv_[i], expected[i].SerializeAsString());
    }
  }

  void ClearMsgs() { msgs_recv_.clear(); }

  bool capture_msgs_;
  bool enable_multinet_;
  IpHelperMessage android_announce_msg_;
  IpHelperMessage legacy_android_announce_msg_;

 private:
  void RecvMsg(const IpHelperMessage& msg) {
    if (capture_msgs_)
      msgs_recv_.emplace_back(msg.SerializeAsString());
  }

  std::vector<std::string> msgs_recv_;
};

}  // namespace

TEST_F(DeviceManagerTest, CtorAddsAndroidDevice) {
  capture_msgs_ = true;
  auto mgr = NewManager();
  VerifyMsgs({android_announce_msg_});
}

TEST_F(DeviceManagerTest, CtorAddsLegacyAndroidDevice_MultinetDisabled) {
  capture_msgs_ = true;
  enable_multinet_ = false;
  auto mgr = NewManager();
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
  enable_multinet_ = false;
  auto mgr = NewManager();
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

TEST_F(DeviceManagerTest, CanDisableAndroidDevice_MultinetDisabled) {
  enable_multinet_ = false;
  auto mgr = NewManager();
  capture_msgs_ = true;
  EXPECT_TRUE(mgr->Disable());
  IpHelperMessage clear_msg;
  clear_msg.set_dev_ifname(kAndroidLegacyDevice);
  clear_msg.set_clear_arc_ip(true);
  IpHelperMessage disable_msg;
  disable_msg.set_dev_ifname(kAndroidLegacyDevice);
  disable_msg.set_disable_inbound(true);
  VerifyMsgs({clear_msg, disable_msg});
}

}  // namespace arc_networkd
