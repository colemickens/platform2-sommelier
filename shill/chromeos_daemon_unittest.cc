// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <vector>

#include <base/bind.h>
#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/chromeos_daemon.h"
#include "shill/dhcp/mock_dhcp_provider.h"
#include "shill/logging.h"
#include "shill/mock_control.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_routing_table.h"
#include "shill/net/io_handler.h"
#include "shill/net/mock_rtnl_handler.h"
#include "shill/net/ndisc.h"
#include "shill/shill_test_config.h"

#if !defined(DISABLE_WIFI)
#include "shill/net/mock_netlink_manager.h"
#include "shill/net/nl80211_message.h"
#endif  // DISABLE_WIFI

using base::Bind;
using base::Callback;
using base::Unretained;
using std::string;
using std::vector;

using ::testing::Expectation;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Test;
using ::testing::_;

namespace shill {

// TODO(zqiu): extend chromeos::Daemon for initializing the message loop once
// we remove the message loop initialization in EventDispatcher.
class TestChromeosDaemon : public ChromeosDaemon {
 public:
  TestChromeosDaemon(const Settings& setttings,
                     Config* config,
                     ControlInterface* control)
      : ChromeosDaemon(Settings(), config, control) {}
  virtual ~TestChromeosDaemon() {}

  MOCK_METHOD0(RunMessageLoop, void());
};

class ChromeosDaemonTest : public Test {
 public:
  ChromeosDaemonTest()
      : daemon_(ChromeosDaemon::Settings(), &config_, new MockControl()),
        metrics_(new MockMetrics(&daemon_.dispatcher_)),
        manager_(new MockManager(daemon_.control_.get(),
                                 &daemon_.dispatcher_,
                                 metrics_,
                                 &daemon_.glib_)),
        dispatcher_(&daemon_.dispatcher_),
        device_info_(daemon_.control_.get(), dispatcher_, metrics_,
                     daemon_.manager_.get()) {
  }
  virtual ~ChromeosDaemonTest() {}
  virtual void SetUp() {
    // Tests initialization done by the daemon's constructor
    ASSERT_NE(nullptr, daemon_.config_);
    ASSERT_NE(nullptr, daemon_.control_.get());
    daemon_.rtnl_handler_ = &rtnl_handler_;
    daemon_.routing_table_ = &routing_table_;
    daemon_.dhcp_provider_ = &dhcp_provider_;
    daemon_.metrics_.reset(metrics_);  // Passes ownership
    daemon_.manager_.reset(manager_);  // Passes ownership

#if !defined(DISABLE_WIFI)
    daemon_.netlink_manager_ = &netlink_manager_;
    const uint16_t kNl80211MessageType = 42;  // Arbitrary.
    ON_CALL(netlink_manager_, GetFamily(Nl80211Message::kMessageTypeString, _)).
            WillByDefault(Return(kNl80211MessageType));
#endif  // DISABLE_WIFI
  }
  void StartDaemon() {
    daemon_.Start();
  }

  void StopDaemon() {
    daemon_.Stop();
  }

  // TODO(zqiu): temporary until we remove message loop initialization in
  // EventDispatcher.
  void RunDaemon() {
    dispatcher_->DispatchForever();
  }

  void ApplySettings(const ChromeosDaemon::Settings& settings) {
    daemon_.ApplySettings(settings);
  }

  MOCK_METHOD0(TerminationAction, void());

 protected:
  TestConfig config_;
  TestChromeosDaemon daemon_;
  MockRTNLHandler rtnl_handler_;
  MockRoutingTable routing_table_;
  MockDHCPProvider dhcp_provider_;
  MockMetrics* metrics_;
  MockManager* manager_;
#if !defined(DISABLE_WIFI)
  MockNetlinkManager netlink_manager_;
#endif  // DISABLE_WIFI
  EventDispatcher* dispatcher_;
  DeviceInfo device_info_;
};

TEST_F(ChromeosDaemonTest, StartStop) {
  // To ensure we do not have any stale routes, we flush a device's routes
  // when it is started.  This requires that the routing table is fully
  // populated before we create and start devices.  So test to make sure that
  // the RoutingTable starts before the Manager (which in turn starts
  // DeviceInfo who is responsible for creating and starting devices).
  // The result is that we request the dump of the routing table and when that
  // completes, we request the dump of the links.  For each link found, we
  // create and start the device.
  EXPECT_CALL(*metrics_, Start());
  EXPECT_CALL(rtnl_handler_, Start(
      RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE |
      RTMGRP_IPV6_IFADDR | RTMGRP_IPV6_ROUTE | RTMGRP_ND_USEROPT));
  Expectation routing_table_started = EXPECT_CALL(routing_table_, Start());
  EXPECT_CALL(dhcp_provider_, Init(_, _, _, _));
  EXPECT_CALL(*manager_, Start()).After(routing_table_started);
  StartDaemon();
  Mock::VerifyAndClearExpectations(metrics_);
  Mock::VerifyAndClearExpectations(manager_);

  EXPECT_CALL(*manager_, Stop());
  EXPECT_CALL(*metrics_, Stop());
  StopDaemon();
}

ACTION_P2(CompleteAction, manager, name) {
  manager->TerminationActionComplete(name);
}

TEST_F(ChromeosDaemonTest, Quit) {
  // This expectation verifies that the termination actions are invoked.
  EXPECT_CALL(*this, TerminationAction())
      .WillOnce(CompleteAction(manager_, "daemon test"));

  manager_->AddTerminationAction("daemon test",
                                 Bind(&ChromeosDaemonTest::TerminationAction,
                                      Unretained(this)));

  // Run Daemon::Quit() after the daemon starts running.
  dispatcher_->PostTask(Bind(&ChromeosDaemon::Quit, Unretained(&daemon_)));

  RunDaemon();
}

TEST_F(ChromeosDaemonTest, ApplySettings) {
  ChromeosDaemon::Settings settings;
  EXPECT_CALL(*manager_, AddDeviceToBlackList(_)).Times(0);
  vector<string> kEmptyStringList;
  EXPECT_CALL(*manager_, SetDHCPv6EnabledDevices(kEmptyStringList));
  EXPECT_CALL(*manager_, SetTechnologyOrder("", _));
  EXPECT_CALL(*manager_, SetIgnoreUnknownEthernet(false));
  EXPECT_CALL(*manager_, SetStartupPortalList(_)).Times(0);
  EXPECT_CALL(*manager_, SetPassiveMode()).Times(0);
  EXPECT_CALL(*manager_, SetPrependDNSServers(""));
  EXPECT_CALL(*manager_, SetMinimumMTU(_)).Times(0);
  EXPECT_CALL(*manager_, SetAcceptHostnameFrom(""));
  ApplySettings(settings);
  Mock::VerifyAndClearExpectations(manager_);

  settings.device_blacklist = {"eth0", "eth1"};
  settings.default_technology_order = "wifi,ethernet";
  vector<string> kDHCPv6EnabledDevices {"eth2", "eth3"};
  settings.dhcpv6_enabled_devices = kDHCPv6EnabledDevices;
  settings.ignore_unknown_ethernet = false;
  settings.portal_list = "wimax";
  settings.use_portal_list = true;
  settings.passive_mode = true;
  settings.prepend_dns_servers = "8.8.8.8,8.8.4.4";
  settings.minimum_mtu = 256;
  settings.accept_hostname_from = "eth*";
  EXPECT_CALL(*manager_, AddDeviceToBlackList("eth0"));
  EXPECT_CALL(*manager_, AddDeviceToBlackList("eth1"));
  EXPECT_CALL(*manager_, SetDHCPv6EnabledDevices(kDHCPv6EnabledDevices));
  EXPECT_CALL(*manager_, SetTechnologyOrder("wifi,ethernet", _));
  EXPECT_CALL(*manager_, SetIgnoreUnknownEthernet(false));
  EXPECT_CALL(*manager_, SetStartupPortalList("wimax"));
  EXPECT_CALL(*manager_, SetPassiveMode());
  EXPECT_CALL(*manager_, SetPrependDNSServers("8.8.8.8,8.8.4.4"));
  EXPECT_CALL(*manager_, SetMinimumMTU(256));
  EXPECT_CALL(*manager_, SetAcceptHostnameFrom("eth*"));
  ApplySettings(settings);
  Mock::VerifyAndClearExpectations(manager_);
}

}  // namespace shill
