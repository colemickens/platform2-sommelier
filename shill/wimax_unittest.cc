// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax.h"

#include <string>

#include <base/stringprintf.h>
#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_wimax_device_proxy.h"
#include "shill/mock_wimax_provider.h"
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

}  // namespace

class WiMaxTest : public testing::Test {
 public:
  WiMaxTest()
      : proxy_(new MockWiMaxDeviceProxy()),
        proxy_factory_(this),
        manager_(&control_, &dispatcher_, &metrics_, NULL),
        device_(new WiMax(&control_, &dispatcher_, &metrics_, &manager_,
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

   private:
    WiMaxTest *test_;

    DISALLOW_COPY_AND_ASSIGN(TestProxyFactory);
  };

  virtual void SetUp() {
    device_->proxy_factory_ = &proxy_factory_;
  }

  virtual void TearDown() {
    device_->SelectService(NULL);
    device_->pending_service_ = NULL;
    device_->proxy_factory_ = NULL;
  }

  scoped_ptr<MockWiMaxDeviceProxy> proxy_;
  TestProxyFactory proxy_factory_;
  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockManager manager_;
  WiMaxRefPtr device_;
};

TEST_F(WiMaxTest, Constructor) {
  EXPECT_EQ(kTestPath, device_->path());
  EXPECT_FALSE(device_->scanning());
}

TEST_F(WiMaxTest, TechnologyIs) {
  EXPECT_TRUE(device_->TechnologyIs(Technology::kWiMax));
  EXPECT_FALSE(device_->TechnologyIs(Technology::kEthernet));
}

TEST_F(WiMaxTest, StartStop) {
  EXPECT_FALSE(device_->proxy_.get());
  EXPECT_CALL(*proxy_, Enable(_, _, _));
  EXPECT_CALL(*proxy_, set_networks_changed_callback(_));
  EXPECT_CALL(*proxy_, Disable(_, _, _));
  device_->Start(NULL, EnabledStateChangedCallback());
  ASSERT_TRUE(device_->proxy_.get());

  device_->networks_.insert("path");
  MockWiMaxProvider provider;
  EXPECT_CALL(manager_, wimax_provider()).WillOnce(Return(&provider));
  EXPECT_CALL(provider, OnNetworksChanged());
  device_->Stop(NULL, EnabledStateChangedCallback());
  EXPECT_TRUE(device_->networks_.empty());
}

TEST_F(WiMaxTest, OnServiceStopped) {
  scoped_refptr<MockWiMaxService> service0(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_));
  scoped_refptr<MockWiMaxService> service1(
      new MockWiMaxService(&control_, NULL, &metrics_, &manager_));
  device_->SelectService(service0);
  device_->pending_service_ = service1;

  device_->OnServiceStopped(NULL);
  EXPECT_TRUE(device_->selected_service());
  EXPECT_TRUE(device_->pending_service_);

  device_->OnServiceStopped(service0);
  EXPECT_FALSE(device_->selected_service());
  EXPECT_TRUE(device_->pending_service_);

  device_->OnServiceStopped(service1);
  EXPECT_FALSE(device_->selected_service());
  EXPECT_FALSE(device_->pending_service_);
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
  EXPECT_TRUE(ContainsKey(device_->networks_, "bar"));
  EXPECT_TRUE(ContainsKey(device_->networks_, "zoo"));
}

}  // namespace shill
