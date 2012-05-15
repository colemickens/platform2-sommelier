// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax.h"

#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_wimax_device_proxy.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using std::string;

namespace shill {

namespace {

const char kTestLinkName[] = "wm0";
const int kTestInterfaceIndex = 5;
const char kTestPath[] = "/org/chromium/WiMaxManager/Device/6";

}  // namespace

class WiMaxTest : public testing::Test {
 public:
  WiMaxTest()
      : proxy_(new MockWiMaxDeviceProxy()),
        proxy_factory_(this),
        manager_(&control_, &dispatcher_, &metrics_, &glib_),
        wimax_(new WiMax(&control_, &dispatcher_, &metrics_, &manager_,
                         kTestLinkName, kTestInterfaceIndex, kTestPath)) {}

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
    wimax_->proxy_factory_ = &proxy_factory_;
  }

  virtual void TearDown() {
    wimax_->proxy_factory_ = NULL;
  }

  scoped_ptr<MockWiMaxDeviceProxy> proxy_;
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
}

TEST_F(WiMaxTest, TechnologyIs) {
  EXPECT_TRUE(wimax_->TechnologyIs(Technology::kWiMax));
  EXPECT_FALSE(wimax_->TechnologyIs(Technology::kEthernet));
}

TEST_F(WiMaxTest, StartStop) {
  EXPECT_FALSE(wimax_->proxy_.get());
  wimax_->Start(NULL, EnabledStateChangedCallback());
  EXPECT_TRUE(wimax_->proxy_.get());
  wimax_->Stop(NULL, EnabledStateChangedCallback());
  EXPECT_FALSE(wimax_->proxy_.get());
}

}  // namespace shill
