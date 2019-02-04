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

    auto dev = Device::ForInterface(
        kAndroidDevice,
        base::Bind(&DeviceManagerTest::RecvMsg, base::Unretained(this)));
    android_announce_msg_.set_dev_ifname(kAndroidDevice);
    *android_announce_msg_.mutable_dev_config() = dev->config();
  }

  std::unique_ptr<DeviceManager> NewManager() {
    return std::make_unique<DeviceManager>(
        base::Bind(&DeviceManagerTest::RecvMsg, base::Unretained(this)));
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

TEST_F(DeviceManagerTest, AddNewDevices) {
  auto mgr = NewManager();
  EXPECT_TRUE(mgr->Add("eth0"));
}

TEST_F(DeviceManagerTest, CannotAddExistingDevice) {
  auto mgr = NewManager();
  EXPECT_FALSE(mgr->Add(kAndroidDevice));
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

TEST_F(DeviceManagerTest, CannotEnableNonAndroidDevice) {
  auto mgr = NewManager();
  EXPECT_FALSE(mgr->Enable("eth0", "eth0"));
}

TEST_F(DeviceManagerTest, CannotDisableNonAndroidDevice) {
  auto mgr = NewManager();
  EXPECT_FALSE(mgr->Disable("eth0"));
}

TEST_F(DeviceManagerTest, CanDisableAndroidDevice) {
  auto mgr = NewManager();
  capture_msgs_ = true;
  EXPECT_TRUE(mgr->Disable(kAndroidDevice));
  IpHelperMessage clear_msg;
  clear_msg.set_dev_ifname(kAndroidDevice);
  clear_msg.set_clear_arc_ip(true);
  IpHelperMessage disable_msg;
  disable_msg.set_dev_ifname(kAndroidDevice);
  disable_msg.set_disable_inbound(true);
  VerifyMsgs({clear_msg, disable_msg});
}

TEST_F(DeviceManagerTest, DisabllAllDisablesAndroidDevice) {
  auto mgr = NewManager();
  capture_msgs_ = true;
  mgr->DisableAll();
  IpHelperMessage clear_msg;
  clear_msg.set_dev_ifname(kAndroidDevice);
  clear_msg.set_clear_arc_ip(true);
  IpHelperMessage disable_msg;
  disable_msg.set_dev_ifname(kAndroidDevice);
  disable_msg.set_disable_inbound(true);
  VerifyMsgs({clear_msg, disable_msg});
}

}  // namespace arc_networkd
