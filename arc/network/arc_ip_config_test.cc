// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/arc_ip_config.h"

#include <utility>
#include <vector>

#include <base/strings/string_util.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "arc/network/fake_process_runner.h"
#include "arc/network/mock_datapath.h"

using testing::_;
using testing::Return;
using testing::StrEq;

namespace arc_networkd {

class ArcIpConfigTest : public testing::Test {
 protected:
  void SetUp() {
    dc_.set_br_ifname("br");
    dc_.set_br_ipv4("1.2.3.4");
    dc_.set_arc_ifname("arc");
    dc_.set_arc_ipv4("6.7.8.9");
    dc_.set_mac_addr("00:11:22:33:44:55");
    android_dc_.set_br_ifname("arcbr0");
    android_dc_.set_br_ipv4("100.115.92.1");
    android_dc_.set_arc_ifname("arc0");
    android_dc_.set_arc_ipv4("100.115.92.2");
    android_dc_.set_mac_addr("00:FF:AA:00:00:56");
    legacy_android_dc_.set_br_ifname("arcbr0");
    legacy_android_dc_.set_br_ipv4("100.115.92.1");
    legacy_android_dc_.set_arc_ifname("arc0");
    legacy_android_dc_.set_arc_ipv4("100.115.92.2");
    legacy_android_dc_.set_mac_addr("00:FF:AA:00:00:56");
    legacy_android_dc_.set_fwd_multicast(true);
    fpr_ = std::make_unique<FakeProcessRunner>();
    runner_ = fpr_.get();
    runner_->Capture(false);
    dp_ = std::make_unique<MockDatapath>(runner_);
    datapath_ = dp_.get();
  }

  std::unique_ptr<ArcIpConfig> Config() {
    return std::make_unique<ArcIpConfig>("eth0", dc_, std::move(fpr_),
                                         std::move(dp_));
  }

  std::unique_ptr<ArcIpConfig> AndroidConfig() {
    return std::make_unique<ArcIpConfig>(kAndroidDevice, android_dc_,
                                         std::move(fpr_), std::move(dp_));
  }

  std::unique_ptr<ArcIpConfig> LegacyAndroidConfig() {
    return std::make_unique<ArcIpConfig>(kAndroidLegacyDevice,
                                         legacy_android_dc_, std::move(fpr_),
                                         std::move(dp_));
  }

 protected:
  FakeProcessRunner* runner_;  // Owned by |fpr_|
  MockDatapath* datapath_;     // Owned by |dp_|

 private:
  DeviceConfig dc_;
  DeviceConfig android_dc_;
  DeviceConfig legacy_android_dc_;
  std::unique_ptr<MockDatapath> dp_;
  std::unique_ptr<FakeProcessRunner> fpr_;
  std::vector<std::string> runs_;
};

TEST_F(ArcIpConfigTest, VerifySetupTeardownDatapath) {
  EXPECT_CALL(*datapath_, AddBridge(StrEq("br"), StrEq("1.2.3.4")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_, AddInboundIPv4DNAT(StrEq("eth0"), StrEq("6.7.8.9")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_, AddOutboundIPv4(StrEq("br"))).WillOnce(Return(true));
  EXPECT_CALL(*datapath_, RemoveBridge(StrEq("br")));

  Config();
}

TEST_F(ArcIpConfigTest, VerifySetupTeardownDatapathForAndroidDevice) {
  EXPECT_CALL(*datapath_, AddBridge(StrEq("arcbr0"), StrEq("100.115.92.1")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_, RemoveBridge(StrEq("arcbr0")));

  AndroidConfig();
}

TEST_F(ArcIpConfigTest, VerifySetupTeardownDatapathForLegacyAndroidDevice) {
  EXPECT_CALL(*datapath_, AddBridge(StrEq("arcbr0"), StrEq("100.115.92.1")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_, AddLegacyIPv4DNAT(StrEq("100.115.92.2")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_, AddOutboundIPv4(StrEq("arcbr0")))
      .WillOnce(Return(true));
  EXPECT_CALL(*datapath_, RemoveOutboundIPv4(StrEq("arcbr0")));
  EXPECT_CALL(*datapath_, RemoveLegacyIPv4DNAT());
  EXPECT_CALL(*datapath_, RemoveBridge(StrEq("arcbr0")));

  LegacyAndroidConfig();
}

TEST_F(ArcIpConfigTest, VerifyInitCmds) {
  auto cfg = Config();
  runner_->Capture(true);
  cfg->Init(12345);
  runner_->VerifyRuns({
      "/bin/ip link delete veth_eth0",
      "/bin/ip link add veth_eth0 type veth peer name peer_eth0",
      "/bin/ifconfig veth_eth0 up",
      "/bin/ip link set dev peer_eth0 addr 00:11:22:33:44:55 down",
      "/sbin/brctl addif br veth_eth0",
      "/bin/ip link set peer_eth0 netns 12345",
  });
  runner_->VerifyAddInterface("peer_eth0", "arc", "6.7.8.9", "255.255.255.252",
                              false, "12345");
}

TEST_F(ArcIpConfigTest, VerifyInitCmdsForAndroidDevice) {
  auto cfg = AndroidConfig();
  runner_->Capture(true);
  cfg->Init(12345);
  runner_->VerifyRuns({
      "/bin/ip link delete veth_arc0",
      "/bin/ip link add veth_arc0 type veth peer name peer_arc0",
      "/bin/ifconfig veth_arc0 up",
      "/bin/ip link set dev peer_arc0 addr 00:FF:AA:00:00:56 down",
      "/sbin/brctl addif arcbr0 veth_arc0",
      "/bin/ip link set peer_arc0 netns 12345",
  });
  runner_->VerifyAddInterface("peer_arc0", "arc0", "100.115.92.2",
                              "255.255.255.252", false, "12345");
  runner_->VerifyWriteSentinel("12345");
}

TEST_F(ArcIpConfigTest, VerifyInitCmdsForLegacyAndroidDevice) {
  auto cfg = LegacyAndroidConfig();
  runner_->Capture(true);
  cfg->Init(12345);
  runner_->VerifyRuns({
      "/bin/ip link delete veth_android",
      "/bin/ip link add veth_android type veth peer name peer_android",
      "/bin/ifconfig veth_android up",
      "/bin/ip link set dev peer_android addr 00:FF:AA:00:00:56 down",
      "/sbin/brctl addif arcbr0 veth_android",
      "/bin/ip link set peer_android netns 12345",
  });
  runner_->VerifyAddInterface("peer_android", "arc0", "100.115.92.2",
                              "255.255.255.252", true, "12345");
  runner_->VerifyWriteSentinel("12345");
}

TEST_F(ArcIpConfigTest, VerifyUninitDoesNotDownLink) {
  auto cfg = Config();
  runner_->Capture(true);
  cfg->Init(0);
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, VerifyContainerReadySendsEnableIfPending) {
  auto cfg = LegacyAndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->ContainerReady(true);
  runner_->VerifyRuns({
      "/sbin/iptables -t nat -A try_arc -i eth0 -j dnat_arc -w",
  });
}

TEST_F(ArcIpConfigTest,
       VerifyContainerReadyDoesNotEnableMultinetAndroidDevice) {
  auto cfg = AndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->ContainerReady(true);
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, VerifyContainerReadyDoesNotEnableRegularDevice) {
  auto cfg = Config();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->ContainerReady(true);
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, VerifyContainerReadySendsEnableOnlyOnce) {
  auto cfg = LegacyAndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->ContainerReady(true);
  cfg->ContainerReady(true);
  cfg->ContainerReady(true);
  runner_->VerifyRuns({
      "/sbin/iptables -t nat -A try_arc -i eth0 -j dnat_arc -w",
  });
}

TEST_F(ArcIpConfigTest, VerifyContainerReadyResendsIfReset) {
  auto cfg = LegacyAndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->ContainerReady(true);
  cfg->ContainerReady(false);
  cfg->EnableInbound("eth0");
  cfg->ContainerReady(true);
  runner_->VerifyRuns({
      "/sbin/iptables -t nat -A try_arc -i eth0 -j dnat_arc -w",
      "/sbin/iptables -t nat -F try_arc -w",
      "/sbin/iptables -t nat -A try_arc -i eth0 -j dnat_arc -w",
  });
}

TEST_F(ArcIpConfigTest, VerifyContainerReadySendsNothingByDefault) {
  auto cfg = LegacyAndroidConfig();
  runner_->Capture(true);
  cfg->ContainerReady(true);
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, VerifyEnableInboundOnlySendsIfContainerReady) {
  auto cfg = LegacyAndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, VerifyMultipleEnableInboundOnlySendsLast) {
  auto cfg = LegacyAndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->EnableInbound("wlan0");
  cfg->ContainerReady(true);
  runner_->VerifyRuns({
      "/sbin/iptables -t nat -A try_arc -i wlan0 -j dnat_arc -w",
  });
}

TEST_F(ArcIpConfigTest, VerifyEnableInboundDisablesFirst) {
  auto cfg = LegacyAndroidConfig();
  cfg->ContainerReady(true);
  cfg->EnableInbound("eth0");
  runner_->Capture(true);
  cfg->EnableInbound("wlan0");
  runner_->VerifyRuns({
      "/sbin/iptables -t nat -F try_arc -w",
      "/sbin/iptables -t nat -A try_arc -i wlan0 -j dnat_arc -w",
  });
}

TEST_F(ArcIpConfigTest, VerifyDisableInboundCmds) {
  auto cfg = LegacyAndroidConfig();
  // Must be enabled first.
  cfg->ContainerReady(true);
  cfg->EnableInbound("eth0");
  runner_->Capture(true);
  cfg->DisableInbound();
  runner_->VerifyRuns({
      "/sbin/iptables -t nat -F try_arc -w",
  });
}

TEST_F(ArcIpConfigTest, VerifyDisableInboundDoesNothingOnNonLegacyAndroid) {
  auto cfg = AndroidConfig();
  // Must be enabled first.
  cfg->ContainerReady(true);
  cfg->EnableInbound("eth0");
  runner_->Capture(true);
  cfg->DisableInbound();
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, VerifyDisableInboundDoesNothingOnRegularDevice) {
  auto cfg = Config();
  // Must be enabled first.
  cfg->ContainerReady(true);
  cfg->EnableInbound("eth0");
  runner_->Capture(true);
  cfg->DisableInbound();
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, DisableDisabledDoesNothing) {
  auto cfg = LegacyAndroidConfig();
  runner_->Capture(true);
  cfg->DisableInbound();
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, VerifyEnableDisableClearsPendingInbound) {
  auto cfg = LegacyAndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->DisableInbound();
  cfg->ContainerReady(true);
  runner_->VerifyRuns({});
}

}  // namespace arc_networkd
