//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <stdint.h>

#include <memory>
#include <vector>

#include <base/bind.h>
#include <base/cancelable_callback.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop_proxy.h>
#include <base/strings/stringprintf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/dhcp/mock_dhcp_provider.h"
#include "shill/logging.h"
#include "shill/mock_control.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_routing_table.h"
#include "shill/net/io_handler.h"
#include "shill/net/mock_rtnl_handler.h"
#include "shill/net/ndisc.h"
#include "shill/shill_daemon.h"
#include "shill/shill_test_config.h"

#if !defined(DISABLE_WIFI)
#include "shill/net/mock_netlink_manager.h"
#include "shill/net/nl80211_message.h"
#endif  // DISABLE_WIFI

using base::Bind;
using base::Callback;
using base::CancelableClosure;
using base::Unretained;
using base::WeakPtrFactory;
using std::string;
using std::vector;

using ::testing::Expectation;
using ::testing::Gt;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Test;
using ::testing::_;

namespace shill {

class MockEventDispatchTester {
 public:
  explicit MockEventDispatchTester(EventDispatcher* dispatcher)
      : dispatcher_(dispatcher),
        triggered_(false),
        callback_count_(0),
        got_data_(false),
        got_ready_(false),
        tester_factory_(this) {
  }

  void ScheduleFailSafe() {
    // Set up a failsafe, so the test still exits even if something goes
    // wrong.
    failsafe_.Reset(Bind(&MockEventDispatchTester::StopDispatcher,
                         tester_factory_.GetWeakPtr()));
    dispatcher_->PostDelayedTask(failsafe_.callback(), 100);
  }

  void ScheduleTimedTasks() {
    dispatcher_->PostDelayedTask(
        Bind(&MockEventDispatchTester::Trigger, tester_factory_.GetWeakPtr()),
        10);
  }

  void RescheduleUnlessTriggered() {
    ++callback_count_;
    if (!triggered_) {
      dispatcher_->PostTask(
          Bind(&MockEventDispatchTester::RescheduleUnlessTriggered,
               tester_factory_.GetWeakPtr()));
    } else {
      failsafe_.Cancel();
      StopDispatcher();
    }
  }

  void StopDispatcher() {
    dispatcher_->PostTask(base::MessageLoop::QuitClosure());
  }

  void Trigger() {
    LOG(INFO) << "MockEventDispatchTester handling " << callback_count_;
    CallbackComplete(callback_count_);
    triggered_ = true;
  }

  void HandleData(InputData* inputData) {
    LOG(INFO) << "MockEventDispatchTester handling data len "
              << base::StringPrintf("%zd %.*s", inputData->len,
                                    static_cast<int>(inputData->len),
                                    inputData->buf);
    got_data_ = true;
    IOComplete(inputData->len);
    StopDispatcher();
  }

  bool GetData() { return got_data_; }

  void ListenIO(int fd) {
    data_callback_ = Bind(&MockEventDispatchTester::HandleData,
                          tester_factory_.GetWeakPtr());
    input_handler_.reset(dispatcher_->CreateInputHandler(
        fd, data_callback_, IOHandler::ErrorCallback()));
  }

  void StopListenIO() {
    got_data_ = false;
    input_handler_.reset(nullptr);
  }

  void HandleReady(int fd) {
    // Stop event handling after we receive in input-ready event.  We should
    // no longer be called until events are re-enabled.
    input_handler_->Stop();

    if (got_ready_) {
      // If we're still getting events after we have stopped them, something
      // is really wrong, and we cannot just depend on ASSERT_FALSE() to get
      // us out of it.  Make sure the dispatcher is also stopped, or else we
      // could end up never exiting.
      StopDispatcher();
      ASSERT_FALSE(got_ready_) << "failed to stop Input Ready events";
    }
    got_ready_ = true;

    LOG(INFO) << "MockEventDispatchTester handling ready for fd " << fd;
    IOComplete(callback_count_);

    if (callback_count_) {
      StopDispatcher();
    } else {
      // Restart Ready events after 10 millisecond delay.
      callback_count_++;
      dispatcher_->PostDelayedTask(
          Bind(&MockEventDispatchTester::RestartReady,
               tester_factory_.GetWeakPtr()),
          10);
    }
  }

  void RestartReady() {
    got_ready_ = false;
    input_handler_->Start();
  }

  void ListenReady(int fd) {
    ready_callback_ = Bind(&MockEventDispatchTester::HandleReady,
                           tester_factory_.GetWeakPtr());
    input_handler_.reset(
        dispatcher_->CreateReadyHandler(fd, IOHandler::kModeInput,
                                        ready_callback_));
  }

  void StopListenReady() {
    got_ready_ = false;
    input_handler_.reset(nullptr);
  }

  MOCK_METHOD1(CallbackComplete, void(int callback_count));
  MOCK_METHOD1(IOComplete, void(int data_length));

 private:
  EventDispatcher* dispatcher_;
  bool triggered_;
  int callback_count_;
  bool got_data_;
  bool got_ready_;
  Callback<void(InputData*)> data_callback_;
  Callback<void(int)> ready_callback_;
  std::unique_ptr<IOHandler> input_handler_;
  WeakPtrFactory<MockEventDispatchTester> tester_factory_;
  CancelableClosure failsafe_;
};

class ShillDaemonTest : public Test {
 public:
  ShillDaemonTest()
      : daemon_(&config_, new MockControl()),  // Passes ownership.
        metrics_(new MockMetrics(&daemon_.dispatcher_)),
        manager_(new MockManager(daemon_.control_.get(),
                                 &daemon_.dispatcher_,
                                 metrics_)),
        dispatcher_(&daemon_.dispatcher_),
        device_info_(daemon_.control_.get(), dispatcher_, metrics_,
                     daemon_.manager_.get()),
        dispatcher_test_(dispatcher_) {
  }
  virtual ~ShillDaemonTest() {}
  virtual void SetUp() {
    // Tests initialization done by the daemon's constructor
    ASSERT_NE(nullptr, daemon_.config_);
    ASSERT_NE(nullptr, daemon_.control_.get());
    daemon_.rtnl_handler_ = &rtnl_handler_;
    daemon_.routing_table_ = &routing_table_;
    daemon_.dhcp_provider_ = &dhcp_provider_;
    daemon_.metrics_.reset(metrics_);  // Passes ownership
    daemon_.manager_.reset(manager_);  // Passes ownership
    dispatcher_test_.ScheduleFailSafe();

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

#if !defined(DISABLE_WIFI)
  void ResetNetlinkManager() {
    daemon_.netlink_manager_->Reset(true);
  }
#endif  // DISABLE_WIFI

  MOCK_METHOD0(TerminationAction, void());

 protected:
  TestConfig config_;
  Daemon daemon_;
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
  StrictMock<MockEventDispatchTester> dispatcher_test_;
};


TEST_F(ShillDaemonTest, StartStop) {
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

TEST_F(ShillDaemonTest, EventDispatcherTimer) {
  EXPECT_CALL(dispatcher_test_, CallbackComplete(Gt(0)));
  dispatcher_test_.ScheduleTimedTasks();
  dispatcher_test_.RescheduleUnlessTriggered();
  dispatcher_->DispatchForever();
}

TEST_F(ShillDaemonTest, EventDispatcherIO) {
  EXPECT_CALL(dispatcher_test_, IOComplete(16));
  int pipefd[2];
  ASSERT_EQ(pipe(pipefd), 0);

  dispatcher_test_.ListenIO(pipefd[0]);
  ASSERT_EQ(write(pipefd[1], "This is a test?!", 16), 16);

  dispatcher_->DispatchForever();
  dispatcher_test_.StopListenIO();
}

TEST_F(ShillDaemonTest, EventDispatcherReady) {
  EXPECT_CALL(dispatcher_test_, IOComplete(0))
      .Times(1);
  EXPECT_CALL(dispatcher_test_, IOComplete(1))
      .Times(1);

  int pipefd[2];
  ASSERT_EQ(pipe(pipefd), 0);

  dispatcher_test_.ListenReady(pipefd[0]);
  ASSERT_EQ(write(pipefd[1], "This is a test?!", 16), 16);

  dispatcher_->DispatchForever();
  dispatcher_test_.StopListenReady();
}

ACTION_P2(CompleteAction, manager, name) {
  manager->TerminationActionComplete(name);
}

TEST_F(ShillDaemonTest, Quit) {
  // The following expectations are to satisfy calls in Daemon::Start().
  EXPECT_CALL(rtnl_handler_, Start(
      RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE |
      RTMGRP_IPV6_IFADDR | RTMGRP_IPV6_ROUTE | RTMGRP_ND_USEROPT));
  EXPECT_CALL(routing_table_, Start());
  EXPECT_CALL(dhcp_provider_, Init(_, _, _, _));
  EXPECT_CALL(*manager_, Start());

  // This expectation verifies that the termination actions are invoked.
  EXPECT_CALL(*this, TerminationAction())
      .WillOnce(CompleteAction(manager_, "daemon test"));

  manager_->AddTerminationAction("daemon test",
                                 Bind(&ShillDaemonTest::TerminationAction,
                                      Unretained(this)));

  // Run Daemon::Quit() after the daemon starts running.
  dispatcher_->PostTask(Bind(&Daemon::Quit, Unretained(&daemon_)));

  daemon_.Run();
#if !defined(DISABLE_WIFI)
  ResetNetlinkManager();
#endif  // DISABLE_WIFI
}

TEST_F(ShillDaemonTest, ApplySettings) {
  Daemon::Settings settings;
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
  daemon_.ApplySettings(settings);
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
  daemon_.ApplySettings(settings);
  Mock::VerifyAndClearExpectations(manager_);
}

}  // namespace shill
