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

#include "shill/wimax/wimax.h"

#include <memory>
#include <string>
#include <utility>

#include "shill/dhcp/mock_dhcp_config.h"
#include "shill/dhcp/mock_dhcp_provider.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/nice_mock_control.h"
#include "shill/test_event_dispatcher.h"
#include "shill/testing.h"
#include "shill/wimax/mock_wimax_device_proxy.h"
#include "shill/wimax/mock_wimax_provider.h"
#include "shill/wimax/mock_wimax_service.h"

using base::Bind;
using base::Unretained;
using std::string;
using testing::_;
using testing::ByMove;
using testing::NiceMock;
using testing::Return;

namespace shill {

namespace {

const char kTestLinkName[] = "wm0";
const char kTestAddress[] = "01:23:45:67:89:ab";
const int kTestInterfaceIndex = 5;
const char kTestPath[] = "/org/chromium/WiMaxManager/Device/6";

}  // namespace

class WiMaxTest : public testing::Test {
 public:
  WiMaxTest()
      : metrics_(&dispatcher_),
        manager_(&control_, &dispatcher_, &metrics_),
        dhcp_config_(new MockDHCPConfig(&control_,
                                        kTestLinkName)),
        device_(new WiMax(&control_, &dispatcher_, &metrics_, &manager_,
                          kTestLinkName, kTestAddress, kTestInterfaceIndex,
                          kTestPath)) {}

  virtual ~WiMaxTest() {}

 protected:
  class Target {
   public:
    virtual ~Target() {}

    MOCK_METHOD1(EnabledStateChanged, void(const Error& error));
  };

  void SetUp() override {
    device_->set_dhcp_provider(&dhcp_provider_);
  }

  void TearDown() override {
    device_->SelectService(nullptr);
    device_->pending_service_ = nullptr;
  }

  NiceMockControl control_;
  EventDispatcherForTest dispatcher_;
  NiceMock<MockMetrics> metrics_;
  MockManager manager_;
  MockDHCPProvider dhcp_provider_;
  scoped_refptr<MockDHCPConfig> dhcp_config_;
  WiMaxRefPtr device_;
};

TEST_F(WiMaxTest, Constructor) {
  EXPECT_EQ(kTestPath, device_->path());
  EXPECT_FALSE(device_->scanning());
}

TEST_F(WiMaxTest, StartStop) {
  auto device_proxy = std::make_unique<MockWiMaxDeviceProxy>();

  EXPECT_CALL(*device_proxy, Enable(_, _, _));
  EXPECT_CALL(*device_proxy, set_networks_changed_callback(_));
  EXPECT_CALL(*device_proxy, set_status_changed_callback(_));
  EXPECT_CALL(*device_proxy, Disable(_, _, _));

  EXPECT_CALL(control_, CreateWiMaxDeviceProxy(_))
      .WillOnce(Return(ByMove(std::move(device_proxy))));

  EXPECT_FALSE(device_->proxy_.get());
  device_->Start(nullptr, EnabledStateChangedCallback());
  ASSERT_TRUE(device_->proxy_.get());

  scoped_refptr<MockWiMaxService> service(
      new MockWiMaxService(&control_, nullptr, &metrics_, &manager_));
  device_->pending_service_ = service;
  EXPECT_CALL(*service, SetState(Service::kStateIdle));
  device_->networks_.insert("path");
  MockWiMaxProvider provider;
  EXPECT_CALL(manager_, wimax_provider()).WillOnce(Return(&provider));
  EXPECT_CALL(provider, OnNetworksChanged());
  device_->StartConnectTimeout();
  device_->Stop(nullptr, EnabledStateChangedCallback());
  EXPECT_TRUE(device_->networks_.empty());
  EXPECT_FALSE(device_->IsConnectTimeoutStarted());
  EXPECT_FALSE(device_->pending_service_.get());
}

TEST_F(WiMaxTest, OnServiceStopped) {
  scoped_refptr<NiceMock<MockWiMaxService>> service0(
      new NiceMock<MockWiMaxService>(&control_, nullptr, &metrics_, &manager_));
  scoped_refptr<MockWiMaxService> service1(
      new MockWiMaxService(&control_, nullptr, &metrics_, &manager_));
  device_->SelectService(service0);
  device_->pending_service_ = service1;

  device_->OnServiceStopped(nullptr);
  EXPECT_TRUE(device_->selected_service().get());
  EXPECT_TRUE(device_->pending_service_.get());

  device_->OnServiceStopped(service0);
  EXPECT_FALSE(device_->selected_service().get());
  EXPECT_TRUE(device_->pending_service_.get());

  device_->OnServiceStopped(service1);
  EXPECT_FALSE(device_->selected_service().get());
  EXPECT_FALSE(device_->pending_service_.get());
}

TEST_F(WiMaxTest, OnNetworksChanged) {
  MockWiMaxProvider provider;
  EXPECT_CALL(manager_, wimax_provider()).WillOnce(Return(&provider));
  EXPECT_CALL(provider, OnNetworksChanged());
  device_->networks_.insert("foo");
  RpcIdentifiers networks;
  networks.push_back("bar");
  networks.push_back("zoo");
  networks.push_back("bar");
  device_->OnNetworksChanged(networks);
  EXPECT_EQ(2, device_->networks_.size());
  EXPECT_TRUE(base::ContainsKey(device_->networks_, "bar"));
  EXPECT_TRUE(base::ContainsKey(device_->networks_, "zoo"));
}

TEST_F(WiMaxTest, OnConnectComplete) {
  scoped_refptr<MockWiMaxService> service(
      new MockWiMaxService(&control_, nullptr, &metrics_, &manager_));
  device_->pending_service_ = service;
  EXPECT_CALL(*service, SetState(_)).Times(0);
  EXPECT_TRUE(device_->pending_service_.get());
  EXPECT_CALL(*service, SetState(Service::kStateFailure));
  device_->OnConnectComplete(Error(Error::kOperationFailed));
  EXPECT_FALSE(device_->pending_service_.get());
}

TEST_F(WiMaxTest, OnStatusChanged) {
  scoped_refptr<MockWiMaxService> service(
      new MockWiMaxService(&control_, nullptr, &metrics_, &manager_));

  EXPECT_EQ(wimax_manager::kDeviceStatusUninitialized, device_->status_);
  device_->pending_service_ = service;
  EXPECT_CALL(*service, SetState(_)).Times(0);
  EXPECT_CALL(*service, ClearPassphrase()).Times(0);
  device_->OnStatusChanged(wimax_manager::kDeviceStatusScanning);
  EXPECT_TRUE(device_->pending_service_.get());
  EXPECT_EQ(wimax_manager::kDeviceStatusScanning, device_->status_);

  device_->status_ = wimax_manager::kDeviceStatusConnecting;
  EXPECT_CALL(*service, SetState(Service::kStateFailure));
  EXPECT_CALL(*service, ClearPassphrase()).Times(0);
  device_->OnStatusChanged(wimax_manager::kDeviceStatusScanning);
  EXPECT_FALSE(device_->pending_service_.get());

  device_->status_ = wimax_manager::kDeviceStatusConnecting;
  device_->SelectService(service);
  EXPECT_CALL(*service, SetState(Service::kStateFailure));
  EXPECT_CALL(*service, SetState(Service::kStateIdle));
  EXPECT_CALL(*service, ClearPassphrase()).Times(0);
  device_->OnStatusChanged(wimax_manager::kDeviceStatusScanning);
  EXPECT_FALSE(device_->selected_service().get());

  device_->pending_service_ = service;
  device_->SelectService(service);
  EXPECT_CALL(*service, SetState(_)).Times(0);
  EXPECT_CALL(*service, ClearPassphrase()).Times(0);
  device_->OnStatusChanged(wimax_manager::kDeviceStatusConnecting);
  EXPECT_TRUE(device_->pending_service_.get());
  EXPECT_TRUE(device_->selected_service().get());
  EXPECT_EQ(wimax_manager::kDeviceStatusConnecting, device_->status_);

  EXPECT_CALL(*service, SetState(Service::kStateIdle));
  device_->SelectService(nullptr);
}

TEST_F(WiMaxTest, UseNoArpGateway) {
  EXPECT_CALL(dhcp_provider_, CreateIPv4Config(kTestLinkName, _, false, _))
      .WillOnce(Return(dhcp_config_));
  device_->AcquireIPConfig();
}

TEST_F(WiMaxTest, DropService) {
  scoped_refptr<NiceMock<MockWiMaxService>> service0(
      new NiceMock<MockWiMaxService>(&control_, nullptr, &metrics_, &manager_));
  scoped_refptr<MockWiMaxService> service1(
      new MockWiMaxService(&control_, nullptr, &metrics_, &manager_));
  device_->SelectService(service0);
  device_->pending_service_ = service1;
  device_->StartConnectTimeout();

  EXPECT_CALL(*service0, SetState(Service::kStateIdle)).Times(2);
  EXPECT_CALL(*service1, SetState(Service::kStateIdle));
  device_->DropService(Service::kStateIdle);
  EXPECT_FALSE(device_->selected_service().get());
  EXPECT_FALSE(device_->pending_service_.get());
  EXPECT_FALSE(device_->IsConnectTimeoutStarted());

  // Expect no crash.
  device_->DropService(Service::kStateFailure);
}

TEST_F(WiMaxTest, OnDeviceVanished) {
  device_->proxy_ = std::make_unique<MockWiMaxDeviceProxy>();
  scoped_refptr<MockWiMaxService> service(
      new MockWiMaxService(&control_, nullptr, &metrics_, &manager_));
  device_->pending_service_ = service;
  EXPECT_CALL(*service, SetState(Service::kStateIdle));
  device_->OnDeviceVanished();
  EXPECT_FALSE(device_->proxy_.get());
  EXPECT_FALSE(device_->pending_service_.get());
}

TEST_F(WiMaxTest, OnEnableComplete) {
  MockWiMaxProvider provider;
  EXPECT_CALL(manager_, wimax_provider()).WillOnce(Return(&provider));
  RpcIdentifiers networks(1, "path");
  auto device_proxy = std::make_unique<MockWiMaxDeviceProxy>();
  EXPECT_CALL(*device_proxy, Networks(_)).WillOnce(Return(networks));
  device_->proxy_ = std::move(device_proxy);

  EXPECT_CALL(provider, OnNetworksChanged());
  Target target;
  EXPECT_CALL(target, EnabledStateChanged(_));
  EnabledStateChangedCallback callback(
      Bind(&Target::EnabledStateChanged, Unretained(&target)));
  Error error;
  device_->OnEnableComplete(callback, error);
  EXPECT_EQ(1, device_->networks_.size());
  EXPECT_TRUE(base::ContainsKey(device_->networks_, "path"));

  EXPECT_TRUE(device_->proxy_.get());
  error.Populate(Error::kOperationFailed);
  EXPECT_CALL(target, EnabledStateChanged(_));
  device_->OnEnableComplete(callback, error);
  EXPECT_FALSE(device_->proxy_.get());
}

TEST_F(WiMaxTest, ConnectTimeout) {
  EXPECT_EQ(&dispatcher_, device_->dispatcher());
  EXPECT_TRUE(device_->connect_timeout_callback_.IsCancelled());
  EXPECT_FALSE(device_->IsConnectTimeoutStarted());
  EXPECT_EQ(WiMax::kDefaultConnectTimeoutSeconds,
            device_->connect_timeout_seconds_);
  device_->connect_timeout_seconds_ = 0;
  device_->StartConnectTimeout();
  EXPECT_FALSE(device_->connect_timeout_callback_.IsCancelled());
  EXPECT_TRUE(device_->IsConnectTimeoutStarted());
  device_->dispatcher_ = nullptr;
  device_->StartConnectTimeout();  // Expect no crash.
  scoped_refptr<MockWiMaxService> service(
      new MockWiMaxService(&control_, nullptr, &metrics_, &manager_));
  device_->pending_service_ = service;
  EXPECT_CALL(*service, SetState(Service::kStateFailure));
  dispatcher_.DispatchPendingEvents();
  EXPECT_TRUE(device_->connect_timeout_callback_.IsCancelled());
  EXPECT_FALSE(device_->IsConnectTimeoutStarted());
  EXPECT_FALSE(device_->pending_service_.get());
}

TEST_F(WiMaxTest, ConnectTo) {
  static const char kPath[] = "/network/path";
  scoped_refptr<MockWiMaxService> service(
      new MockWiMaxService(&control_, nullptr, &metrics_, &manager_));
  EXPECT_CALL(*service, SetState(Service::kStateAssociating));
  device_->status_ = wimax_manager::kDeviceStatusScanning;
  EXPECT_CALL(*service, GetNetworkObjectPath()).WillOnce(Return(kPath));
  auto device_proxy = std::make_unique<MockWiMaxDeviceProxy>();
  EXPECT_CALL(*device_proxy, Connect(kPath, _, _, _, _))
      .WillOnce(SetErrorTypeInArgument<2>(Error::kSuccess));
  device_->proxy_ = std::move(device_proxy);
  Error error;
  device_->ConnectTo(service, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(service.get(), device_->pending_service_.get());
  EXPECT_EQ(wimax_manager::kDeviceStatusUninitialized, device_->status_);
  EXPECT_TRUE(device_->IsConnectTimeoutStarted());

  device_->ConnectTo(service, &error);
  EXPECT_EQ(Error::kInProgress, error.type());

  device_->pending_service_ = nullptr;
}

TEST_F(WiMaxTest, IsIdle) {
  EXPECT_TRUE(device_->IsIdle());
  scoped_refptr<NiceMock<MockWiMaxService>> service(
      new NiceMock<MockWiMaxService>(&control_, nullptr, &metrics_, &manager_));
  device_->pending_service_ = service;
  EXPECT_FALSE(device_->IsIdle());
  device_->pending_service_ = nullptr;
  device_->SelectService(service);
  EXPECT_FALSE(device_->IsIdle());
}

}  // namespace shill
