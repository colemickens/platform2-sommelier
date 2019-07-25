// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/arc_service.h"

#include <utility>
#include <vector>

#include <base/bind.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "arc/network/device_manager.h"
#include "arc/network/fake_process_runner.h"
#include "arc/network/mock_datapath.h"

using testing::_;
using testing::Return;
using testing::StrEq;

namespace arc_networkd {
namespace {
constexpr char kTestPID[] = "-2";

class MockDeviceManager : public DeviceManagerBase {
 public:
  MockDeviceManager() = default;
  ~MockDeviceManager() = default;

  MOCK_METHOD1(RegisterDeviceAddedHandler, void(const DeviceHandler&));
  MOCK_METHOD1(RegisterDeviceRemovedHandler, void(const DeviceHandler&));
  MOCK_METHOD1(RegisterDefaultInterfaceChangedHandler,
               void(const NameHandler&));
  MOCK_METHOD1(RegisterDeviceIPv6AddressFoundHandler,
               void(const DeviceHandler&));
  MOCK_METHOD1(OnGuestStart, void(GuestMessage::GuestType));
  MOCK_METHOD1(OnGuestStop, void(GuestMessage::GuestType));
  MOCK_METHOD1(ProcessDevices, void(const DeviceHandler&));
  MOCK_CONST_METHOD1(Exists, bool(const std::string& name));
  MOCK_CONST_METHOD1(FindByHostInterface, Device*(const std::string& ifname));
  MOCK_CONST_METHOD1(FindByGuestInterface, Device*(const std::string& ifname));
  MOCK_CONST_METHOD0(DefaultInterface, const std::string&());
};

void HandleDeviceMsg(const DeviceMessage&) {}

}  // namespace

class ArcServiceTest : public testing::Test {
 public:
  ArcServiceTest()
      : testing::Test(),
        addr_mgr_({
            AddressManager::Guest::ARC,
            AddressManager::Guest::ARC_NET,
        }) {}

 protected:
  void SetUp() override {
    fpr_ = std::make_unique<FakeProcessRunner>();
    runner_ = fpr_.get();
    runner_->Capture(false);
    dp_ = std::make_unique<MockDatapath>(runner_);
    datapath_ = dp_.get();
  }

  std::unique_ptr<ArcService> NewService(bool arc_legacy = false,
                                         bool valid_pid = true) {
    auto svc =
        std::make_unique<ArcService>(&dev_mgr_, arc_legacy, std::move(dp_));
    if (valid_pid)
      svc->SetPIDForTestingOnly();

    return svc;
  }

  std::unique_ptr<Device> MakeDevice(const std::string& name,
                                     const std::string& host,
                                     const std::string& guest) {
    auto subnet = addr_mgr_.AllocateIPv4Subnet(
        ((name == kAndroidDevice) || (name == kAndroidLegacyDevice))
            ? AddressManager::Guest::ARC
            : AddressManager::Guest::ARC_NET);
    auto addr0 = subnet->AllocateAtOffset(0);
    auto addr1 = subnet->AllocateAtOffset(1);
    auto cfg = std::make_unique<Device::Config>(
        host, guest, addr_mgr_.GenerateMacAddress(), std::move(subnet),
        std::move(addr0), std::move(addr1));
    Device::Options opt{true, true};
    return std::make_unique<Device>(name, std::move(cfg), opt,
                                    base::Bind(HandleDeviceMsg));
  }

  AddressManager addr_mgr_;
  MockDeviceManager dev_mgr_;
  FakeProcessRunner* runner_;  // Owned by |fpr_|
  MockDatapath* datapath_;     // Owned by |dp_|
  std::unique_ptr<MockDatapath> dp_;

 private:
  std::unique_ptr<FakeProcessRunner> fpr_;
};

TEST_F(ArcServiceTest, VerifyOnDeviceAddedDatapathForLegacyAndroid) {
  EXPECT_CALL(*datapath_, AddBridge(StrEq("arcbr0"), StrEq("100.115.92.1")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_, AddLegacyIPv4DNAT(StrEq("100.115.92.2")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_, AddOutboundIPv4(StrEq("arcbr0")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_,
              AddVirtualBridgedInterface(StrEq("android"), _, StrEq("arcbr0")))
      .WillOnce(Return("peer_android"));
  EXPECT_CALL(*datapath_,
              AddInterfaceToContainer(_, StrEq("peer_android"), StrEq("arc0"),
                                      StrEq("100.115.92.2"), true))
      .WillOnce(Return(true));

  auto dev = MakeDevice(kAndroidLegacyDevice, "arcbr0", "arc0");
  ASSERT_TRUE(dev);
  NewService(true)->OnDeviceAdded(dev.get());
  runner_->VerifyWriteSentinel(kTestPID);
}

TEST_F(ArcServiceTest, VerifyOnDeviceAddedDoesNothingLegacyAndroidNoARC) {
  // If ARC N hasn't started yet, nothing should happen.
  auto dev = MakeDevice(kAndroidLegacyDevice, "arcbr0", "arc0");
  ASSERT_TRUE(dev);
  NewService(false, false)->OnDeviceAdded(dev.get());
}

TEST_F(ArcServiceTest,
       VerifyOnDeviceAddedDoesNothingLegacyAndroidOtherInterface) {
  // In ARC N, only the legacy interface is added.
  auto dev = MakeDevice("eth0", "arc_eth0", "eth0");
  ASSERT_TRUE(dev);
  NewService(true)->OnDeviceAdded(dev.get());
}

TEST_F(ArcServiceTest, VerifyOnDeviceRemovedDatapathForLegacyAndroid) {
  EXPECT_CALL(*datapath_, RemoveBridge(StrEq("arcbr0")));

  auto dev = MakeDevice(kAndroidLegacyDevice, "arcbr0", "arc0");
  ASSERT_TRUE(dev);
  NewService(true)->OnDeviceRemoved(dev.get());
}

TEST_F(ArcServiceTest, VerifyOnDeviceAddedDatapathForAndroid) {
  EXPECT_CALL(*datapath_, AddBridge(StrEq("arcbr0"), StrEq("100.115.92.1")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_,
              AddVirtualBridgedInterface(StrEq("arc0"), _, StrEq("arcbr0")))
      .WillOnce(Return("peer_arc0"));
  EXPECT_CALL(*datapath_,
              AddInterfaceToContainer(_, StrEq("peer_arc0"), StrEq("arc0"),
                                      StrEq("100.115.92.2"), true))
      .WillOnce(Return(true));

  auto dev = MakeDevice(kAndroidDevice, "arcbr0", "arc0");
  ASSERT_TRUE(dev);
  NewService()->OnDeviceAdded(dev.get());
  runner_->VerifyWriteSentinel(kTestPID);
}

TEST_F(ArcServiceTest, VerifyOnDeviceRemovedDatapathForAndroid) {
  EXPECT_CALL(*datapath_, RemoveBridge(StrEq("arcbr0")));

  auto dev = MakeDevice(kAndroidDevice, "arcbr0", "arc0");
  ASSERT_TRUE(dev);
  NewService()->OnDeviceRemoved(dev.get());
}

TEST_F(ArcServiceTest, VerifyOnDeviceAddedDatapath) {
  EXPECT_CALL(*datapath_, AddBridge(StrEq("arc_eth0"), StrEq("100.115.92.9")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_,
              AddInboundIPv4DNAT(StrEq("eth0"), StrEq("100.115.92.10")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_, AddOutboundIPv4(StrEq("arc_eth0")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_,
              AddVirtualBridgedInterface(StrEq("eth0"), _, StrEq("arc_eth0")))
      .WillOnce(Return("peer_eth0"));
  EXPECT_CALL(*datapath_,
              AddInterfaceToContainer(_, StrEq("peer_eth0"), StrEq("eth0"),
                                      StrEq("100.115.92.10"), true))
      .WillOnce(Return(true));

  auto dev = MakeDevice("eth0", "arc_eth0", "eth0");
  ASSERT_TRUE(dev);
  NewService()->OnDeviceAdded(dev.get());
}

TEST_F(ArcServiceTest, VerifyOnDeviceRemovedDatapath) {
  EXPECT_CALL(*datapath_, RemoveBridge(StrEq("arc_eth0")));

  auto dev = MakeDevice("eth0", "arc_eth0", "eth0");
  ASSERT_TRUE(dev);
  NewService()->OnDeviceRemoved(dev.get());
}

}  // namespace arc_networkd
