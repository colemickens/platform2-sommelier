// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_provider.h"

#include <gtest/gtest.h>

#include "shill/mock_dbus_properties_proxy.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_wimax_manager_proxy.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using std::string;
using std::vector;
using testing::_;
using testing::Return;

namespace shill {

class WiMaxProviderTest : public testing::Test {
 public:
  WiMaxProviderTest()
      : manager_proxy_(new MockWiMaxManagerProxy()),
        properties_proxy_(new MockDBusPropertiesProxy()),
        proxy_factory_(this),
        manager_(&control_, NULL, &metrics_, NULL),
        provider_(&control_, NULL, &metrics_, &manager_) {}

  virtual ~WiMaxProviderTest() {}

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(WiMaxProviderTest *test) : test_(test) {}

    virtual DBusPropertiesProxyInterface *CreateDBusPropertiesProxy(
        const string &/*path*/, const string &/*service*/) {
      return test_->properties_proxy_.release();
    }

    virtual WiMaxManagerProxyInterface *CreateWiMaxManagerProxy() {
      return test_->manager_proxy_.release();
    }

   private:
    WiMaxProviderTest *test_;

    DISALLOW_COPY_AND_ASSIGN(TestProxyFactory);
  };

  virtual void SetUp() {
    provider_.proxy_factory_ = &proxy_factory_;
  }

  virtual void TearDown() {
    provider_.proxy_factory_ = NULL;
  }

  scoped_ptr<MockWiMaxManagerProxy> manager_proxy_;
  scoped_ptr<MockDBusPropertiesProxy> properties_proxy_;
  TestProxyFactory proxy_factory_;
  NiceMockControl control_;
  MockMetrics metrics_;
  MockManager manager_;
  WiMaxProvider provider_;
};

TEST_F(WiMaxProviderTest, StartStop) {
  EXPECT_FALSE(provider_.manager_proxy_.get());
  EXPECT_FALSE(provider_.properties_proxy_.get());
  vector<RpcIdentifier> devices;
  EXPECT_CALL(*properties_proxy_, set_properties_changed_callback(_)).Times(1);
  EXPECT_CALL(*manager_proxy_, Devices(_)).WillOnce(Return(devices));
  provider_.Start();
  EXPECT_TRUE(provider_.manager_proxy_.get());
  EXPECT_TRUE(provider_.properties_proxy_.get());
  provider_.Start();
  EXPECT_TRUE(provider_.manager_proxy_.get());
  EXPECT_TRUE(provider_.properties_proxy_.get());
  provider_.Stop();
  EXPECT_FALSE(provider_.manager_proxy_.get());
  EXPECT_FALSE(provider_.properties_proxy_.get());
  provider_.Stop();
}

}  // namespace shill
