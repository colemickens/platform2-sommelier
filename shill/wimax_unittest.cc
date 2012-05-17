// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax.h"

#include <base/stringprintf.h>
#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_wimax_device_proxy.h"
#include "shill/mock_wimax_network_proxy.h"
#include "shill/mock_wimax_service.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using std::string;
using testing::_;
using testing::Return;

namespace shill {

namespace {

const char kTestLinkName[] = "wm0";
const char kTestAddress[] = "01:23:45:67:89:ab";
const int kTestInterfaceIndex = 5;
const char kTestPath[] = "/org/chromium/WiMaxManager/Device/6";

string GetTestNetworkPath(uint32 identifier) {
  return base::StringPrintf("%s%08x",
                            wimax_manager::kNetworkObjectPathPrefix,
                            identifier);
}

}  // namespace

class WiMaxTest : public testing::Test {
 public:
  WiMaxTest()
      : proxy_(new MockWiMaxDeviceProxy()),
        network_proxy_(new MockWiMaxNetworkProxy()),
        proxy_factory_(this),
        manager_(&control_, &dispatcher_, &metrics_, &glib_),
        wimax_(new WiMax(&control_, &dispatcher_, &metrics_, &manager_,
                         kTestLinkName, kTestAddress, kTestInterfaceIndex,
                         kTestPath)) {}

  virtual ~WiMaxTest() {}

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(WiMaxTest *test) : test_(test) {}

    virtual WiMaxDeviceProxyInterface *CreateWiMaxDeviceProxy(
        const string &/*path*/) {
      return test_->proxy_.release();
    }

    virtual WiMaxNetworkProxyInterface *CreateWiMaxNetworkProxy(
        const string &/*path*/) {
      return test_->network_proxy_.release();
    }

   private:
    WiMaxTest *test_;

    DISALLOW_COPY_AND_ASSIGN(TestProxyFactory);
  };

  virtual void SetUp() {
    wimax_->proxy_factory_ = &proxy_factory_;
  }

  virtual void TearDown() {
    wimax_->services_.clear();
    wimax_->proxy_factory_ = NULL;
  }

  scoped_ptr<MockWiMaxDeviceProxy> proxy_;
  scoped_ptr<MockWiMaxNetworkProxy> network_proxy_;
  TestProxyFactory proxy_factory_;
  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;
  WiMaxRefPtr wimax_;
};

TEST_F(WiMaxTest, Constructor) {
  EXPECT_EQ(kTestPath, wimax_->path());
  EXPECT_FALSE(wimax_->scanning());
}

TEST_F(WiMaxTest, TechnologyIs) {
  EXPECT_TRUE(wimax_->TechnologyIs(Technology::kWiMax));
  EXPECT_FALSE(wimax_->TechnologyIs(Technology::kEthernet));
}

TEST_F(WiMaxTest, StartStop) {
  EXPECT_FALSE(wimax_->proxy_.get());
  EXPECT_CALL(*proxy_, Enable(_, _, _));
  EXPECT_CALL(*proxy_, set_networks_changed_callback(_));
  EXPECT_CALL(*proxy_, Disable(_, _, _));
  wimax_->Start(NULL, EnabledStateChangedCallback());
  EXPECT_TRUE(wimax_->proxy_.get());
  wimax_->Stop(NULL, EnabledStateChangedCallback());
}

TEST_F(WiMaxTest, OnNetworksChanged) {
  wimax_->services_[GetTestNetworkPath(2)] =
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL);
  RpcIdentifiers live_devices;
  live_devices.push_back(GetTestNetworkPath(3));
  EXPECT_CALL(manager_, DeregisterService(_));
  EXPECT_CALL(*network_proxy_, Name(_));
  EXPECT_CALL(*network_proxy_, Identifier(_));
  EXPECT_CALL(manager_, RegisterService(_));
  wimax_->OnNetworksChanged(live_devices);
  ASSERT_EQ(1, wimax_->services_.size());
  EXPECT_TRUE(ContainsKey(wimax_->services_, GetTestNetworkPath(3)));
}

TEST_F(WiMaxTest, CreateService) {
  EXPECT_TRUE(wimax_->services_.empty());

  static const char kTestNetworkName[] = "TestWiMaxNetwork";

  EXPECT_CALL(*network_proxy_, Name(_)).WillOnce(Return(kTestNetworkName));
  EXPECT_CALL(*network_proxy_, Identifier(_));
  EXPECT_CALL(manager_, RegisterService(_));
  wimax_->CreateService(GetTestNetworkPath(1));
  ASSERT_EQ(1, wimax_->services_.size());
  EXPECT_TRUE(ContainsKey(wimax_->services_, GetTestNetworkPath(1)));
  EXPECT_EQ(kTestNetworkName,
            wimax_->services_[GetTestNetworkPath(1)]->friendly_name());

  WiMaxService *service = wimax_->services_[GetTestNetworkPath(1)];
  wimax_->CreateService(GetTestNetworkPath(1));
  EXPECT_EQ(service, wimax_->services_[GetTestNetworkPath(1)]);
}

TEST_F(WiMaxTest, DestroyDeadServices) {
  for (int i = 100; i < 104; i++) {
    wimax_->services_[GetTestNetworkPath(i)] =
        new MockWiMaxService(&control_, NULL, &metrics_, &manager_, NULL);
  }
  wimax_->SelectService(wimax_->services_[GetTestNetworkPath(100)]);
  wimax_->pending_service_ = wimax_->services_[GetTestNetworkPath(103)];

  RpcIdentifiers live_networks;
  live_networks.push_back(GetTestNetworkPath(777));
  live_networks.push_back(GetTestNetworkPath(100));
  live_networks.push_back(GetTestNetworkPath(103));
  EXPECT_CALL(manager_, DeregisterService(_)).Times(2);
  wimax_->DestroyDeadServices(live_networks);
  ASSERT_EQ(2, wimax_->services_.size());
  EXPECT_TRUE(ContainsKey(wimax_->services_, GetTestNetworkPath(100)));
  EXPECT_TRUE(ContainsKey(wimax_->services_, GetTestNetworkPath(103)));
  EXPECT_TRUE(wimax_->selected_service());
  EXPECT_TRUE(wimax_->pending_service_);

  live_networks.pop_back();
  EXPECT_CALL(manager_, DeregisterService(_)).Times(1);
  wimax_->DestroyDeadServices(live_networks);
  ASSERT_EQ(1, wimax_->services_.size());
  EXPECT_TRUE(ContainsKey(wimax_->services_, GetTestNetworkPath(100)));
  EXPECT_TRUE(wimax_->selected_service());
  EXPECT_FALSE(wimax_->pending_service_);

  live_networks.pop_back();
  EXPECT_CALL(manager_, DeregisterService(_)).Times(1);
  wimax_->DestroyDeadServices(live_networks);
  EXPECT_TRUE(wimax_->services_.empty());
  EXPECT_FALSE(wimax_->selected_service());
}

}  // namespace shill
