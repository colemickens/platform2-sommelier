// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/arc_ip_config.h"

#include <utility>
#include <vector>

#include <base/strings/string_util.h>

#include <gtest/gtest.h>

namespace arc_networkd {

namespace {

class FakeProcessRunner : public MinijailedProcessRunner {
 public:
  explicit FakeProcessRunner(std::vector<std::string>* runs = nullptr)
      : runs_(runs ? runs : &runs_vec_) {}
  ~FakeProcessRunner() = default;

  int Run(const std::vector<std::string>& argv, bool log_failures) override {
    if (capture_)
      runs_->emplace_back(base::JoinString(argv, " "));
    return 0;
  }

  int AddInterfaceToContainer(const std::string& host_ifname,
                              const std::string& con_ifname,
                              const std::string& con_ipv4,
                              const std::string& con_nmask,
                              bool enable_multicast,
                              const std::string& con_pid) override {
    add_host_ifname_ = host_ifname;
    add_con_ifname_ = con_ifname;
    add_con_ipv4_ = con_ipv4;
    add_con_nmask_ = con_nmask;
    add_enable_multicast_ = enable_multicast;
    add_con_pid_ = con_pid;
    return 0;
  }

  int WriteSentinelToContainer(const std::string& con_pid) override {
    wr_con_pid_ = con_pid;
    return 0;
  }

  void Capture(bool on, std::vector<std::string>* runs = nullptr) {
    capture_ = on;
    if (runs)
      runs_ = runs;
  }

  void VerifyRuns(const std::vector<std::string>& expected) {
    VerifyRuns(*runs_, expected);
  }

  static void VerifyRuns(const std::vector<std::string>& got,
                         const std::vector<std::string>& expected) {
    ASSERT_EQ(got.size(), expected.size());
    for (int i = 0; i < got.size(); ++i) {
      EXPECT_EQ(got[i], expected[i]);
    }
  }

  void VerifyAddInterface(const std::string& host_ifname,
                          const std::string& con_ifname,
                          const std::string& con_ipv4,
                          const std::string& con_nmask,
                          bool enable_multicast,
                          const std::string& con_pid) {
    EXPECT_EQ(host_ifname, add_host_ifname_);
    EXPECT_EQ(con_ifname, add_con_ifname_);
    EXPECT_EQ(con_ipv4, add_con_ipv4_);
    EXPECT_EQ(con_nmask, add_con_nmask_);
    EXPECT_EQ(enable_multicast, add_enable_multicast_);
    EXPECT_EQ(con_pid, add_con_pid_);
  }

  void VerifyWriteSentinel(const std::string& con_pid) {
    EXPECT_EQ(con_pid, wr_con_pid_);
  }

 private:
  bool capture_ = false;
  std::vector<std::string>* runs_;
  std::vector<std::string> runs_vec_;
  std::string add_host_ifname_;
  std::string add_con_ifname_;
  std::string add_con_ipv4_;
  std::string add_con_nmask_;
  bool add_enable_multicast_;
  std::string add_con_pid_;
  std::string wr_con_pid_;

  DISALLOW_COPY_AND_ASSIGN(FakeProcessRunner);
};

}  // namespace

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
    android_dc_.set_fwd_multicast(true);
    fpr_ = std::make_unique<FakeProcessRunner>();
    runner_ = fpr_.get();
    runner_->Capture(false);
  }

  std::unique_ptr<ArcIpConfig> Config() {
    return std::make_unique<ArcIpConfig>("foo", dc_, std::move(fpr_));
  }

  std::unique_ptr<ArcIpConfig> AndroidConfig() {
    return std::make_unique<ArcIpConfig>("android", android_dc_,
                                         std::move(fpr_));
  }

 protected:
  FakeProcessRunner* runner_;  // Owned by |fpr_|

 private:
  DeviceConfig dc_;
  DeviceConfig android_dc_;
  std::unique_ptr<FakeProcessRunner> fpr_;
  std::vector<std::string> runs_;
};

TEST_F(ArcIpConfigTest, VerifySetupCmds) {
  // Setup called in ctor.
  runner_->Capture(true);
  auto cfg = Config();
  runner_->VerifyRuns(
      {"/sbin/brctl addbr br",
       "/bin/ifconfig br 1.2.3.4 netmask 255.255.255.252 up",
       "/sbin/iptables -t mangle -A PREROUTING -i br -j MARK --set-mark 1 -w"});
}

TEST_F(ArcIpConfigTest, VerifyTeardownCmds) {
  std::vector<std::string> runs;
  {
    // Teardown called in dtor.
    auto cfg = Config();
    runner_->Capture(true, &runs);
  }
  FakeProcessRunner::VerifyRuns(
      {"/sbin/iptables -t mangle -D PREROUTING -i br -j MARK --set-mark 1 -w",
       "/bin/ifconfig br down", "/sbin/brctl delbr br"},
      runs);
}

TEST_F(ArcIpConfigTest, VerifySetupCmdsForAndroidDevice) {
  runner_->Capture(true);
  auto cfg = AndroidConfig();
  runner_->VerifyRuns(
      {"/sbin/brctl addbr arcbr0",
       "/bin/ifconfig arcbr0 100.115.92.1 netmask 255.255.255.252 up",
       "/sbin/iptables -t mangle -A PREROUTING -i arcbr0 -j MARK --set-mark 1 "
       "-w",
       "/sbin/iptables -t nat -N dnat_arc -w",
       "/sbin/iptables -t nat -A dnat_arc -j DNAT --to-destination "
       "100.115.92.2 -w",
       "/sbin/iptables -t nat -N try_arc -w",
       "/sbin/iptables -t nat -A PREROUTING -m socket --nowildcard -j ACCEPT "
       "-w",
       "/sbin/iptables -t nat -A PREROUTING -p tcp -j try_arc -w",
       "/sbin/iptables -t nat -A PREROUTING -p udp -j try_arc -w",
       "/sbin/iptables -t filter -A FORWARD -o arcbr0 -j ACCEPT -w"});
}

TEST_F(ArcIpConfigTest, VerifyTeardownCmdsForAndroidDevice) {
  std::vector<std::string> runs;
  {
    auto cfg = AndroidConfig();
    runner_->Capture(true, &runs);
  }
  FakeProcessRunner::VerifyRuns(
      {
          "/sbin/iptables -t filter -D FORWARD -o arcbr0 -j ACCEPT -w",
          "/sbin/iptables -t nat -D PREROUTING -p udp -j try_arc -w",
          "/sbin/iptables -t nat -D PREROUTING -p tcp -j try_arc -w",
          "/sbin/iptables -t nat -D PREROUTING -m socket --nowildcard -j "
          "ACCEPT -w",
          "/sbin/iptables -t nat -F try_arc -w",
          "/sbin/iptables -t nat -X try_arc -w",
          "/sbin/iptables -t nat -F dnat_arc -w",
          "/sbin/iptables -t nat -X dnat_arc -w",
          "/sbin/iptables -t mangle -D PREROUTING -i arcbr0 -j MARK --set-mark "
          "1 -w",
          "/bin/ifconfig arcbr0 down",
          "/sbin/brctl delbr arcbr0",
      },
      runs);
}

TEST_F(ArcIpConfigTest, VerifyInitCmds) {
  auto cfg = Config();
  runner_->Capture(true);
  cfg->Init(12345);
  runner_->VerifyRuns({
      "/bin/ip link delete veth_foo",
      "/bin/ip link add veth_foo type veth peer name peer_foo",
      "/bin/ifconfig veth_foo up",
      "/bin/ip link set dev peer_foo addr 00:11:22:33:44:55 down",
      "/sbin/brctl addif br veth_foo",
      "/bin/ip link set peer_foo netns 12345",
  });
  runner_->VerifyAddInterface("peer_foo", "arc", "6.7.8.9", "255.255.255.252",
                              false, "12345");
}

TEST_F(ArcIpConfigTest, VerifyInitCmdsForAndroidDevice) {
  auto cfg = AndroidConfig();
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
  auto cfg = AndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->ContainerReady(true);
  runner_->VerifyRuns({
      "/sbin/iptables -t nat -A try_arc -i eth0 -j dnat_arc -w",
  });
}

TEST_F(ArcIpConfigTest, VerifyContainerReadyOnlyEnablesAndroidDevice) {
  auto cfg = Config();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->ContainerReady(true);
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, VerifyContainerReadySendsEnableOnlyOnce) {
  auto cfg = AndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->ContainerReady(true);
  cfg->ContainerReady(true);
  cfg->ContainerReady(true);
  runner_->VerifyRuns({
      "/sbin/iptables -t nat -A try_arc -i eth0 -j dnat_arc -w",
  });
}

TEST_F(ArcIpConfigTest, VerifyContainerReadySendsNothingByDefault) {
  auto cfg = AndroidConfig();
  runner_->Capture(true);
  cfg->ContainerReady(true);
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, VerifyEnableInboundOnlySendsIfContainerReady) {
  auto cfg = AndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, VerifyMultipleEnableInboundOnlySendsLast) {
  auto cfg = AndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->EnableInbound("wlan0");
  cfg->ContainerReady(true);
  runner_->VerifyRuns({
      "/sbin/iptables -t nat -A try_arc -i wlan0 -j dnat_arc -w",
  });
}

TEST_F(ArcIpConfigTest, VerifyEnableInboundDisablesFirst) {
  auto cfg = AndroidConfig();
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
  auto cfg = AndroidConfig();
  // Must be enabled first.
  cfg->ContainerReady(true);
  cfg->EnableInbound("eth0");
  runner_->Capture(true);
  cfg->DisableInbound();
  runner_->VerifyRuns({
      "/sbin/iptables -t nat -F try_arc -w",
  });
}

TEST_F(ArcIpConfigTest, DisableDisabledDoesNothing) {
  auto cfg = AndroidConfig();
  runner_->Capture(true);
  cfg->DisableInbound();
  runner_->VerifyRuns({});
}

TEST_F(ArcIpConfigTest, VerifyEnableDisableClearsPendingInbound) {
  auto cfg = AndroidConfig();
  runner_->Capture(true);
  cfg->EnableInbound("eth0");
  cfg->DisableInbound();
  cfg->ContainerReady(true);
  runner_->VerifyRuns({});
}

}  // namespace arc_networkd
