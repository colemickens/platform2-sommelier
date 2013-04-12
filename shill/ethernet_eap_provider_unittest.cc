// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet_eap_provider.h"

#include <base/bind.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_control.h"
#include "shill/mock_ethernet.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"

using testing::_;
using testing::Mock;
using testing::SaveArg;

namespace shill {

class EthernetEapProviderTest : public testing::Test {
 public:
  EthernetEapProviderTest()
      : metrics_(&dispatcher_),
        manager_(&control_, &dispatcher_, &metrics_, NULL),
        provider_(&control_, &dispatcher_, &metrics_, &manager_) {}
  virtual ~EthernetEapProviderTest() {}

  MOCK_METHOD0(Callback0, void());
  MOCK_METHOD0(Callback1, void());

 protected:
  const EthernetEapProvider::CallbackMap &CallbackMap() {
    return provider_.callback_map_;
  }

  MockControl control_;
  MockEventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockManager manager_;
  EthernetEapProvider provider_;
};

TEST_F(EthernetEapProviderTest, Construct) {
  EXPECT_EQ(ServiceRefPtr(), provider_.service());
  EXPECT_TRUE(CallbackMap().empty());
}

TEST_F(EthernetEapProviderTest, StartAndStop) {
  ServiceRefPtr service;
  EXPECT_CALL(manager_, RegisterService(_)).WillOnce(SaveArg<0>(&service));
  provider_.Start();
  EXPECT_NE(ServiceRefPtr(), provider_.service());
  EXPECT_EQ(service, provider_.service());

  EXPECT_CALL(manager_, DeregisterService(service));
  provider_.Stop();
  EXPECT_EQ(service, provider_.service());

  // Provider re-uses the same service on restart.
  EXPECT_CALL(manager_, RegisterService(service));
  provider_.Start();
  Mock::VerifyAndClearExpectations(&manager_);
}

TEST_F(EthernetEapProviderTest, CredentialChangeCallback) {
  EXPECT_CALL(*this, Callback0()).Times(0);
  EXPECT_CALL(*this, Callback1()).Times(0);
  provider_.OnCredentialsChanged();

  scoped_refptr<MockEthernet> device0 = new MockEthernet(&control_,
                                                         &dispatcher_,
                                                         &metrics_,
                                                         &manager_,
                                                         "eth0",
                                                         "addr0",
                                                         0);
  EthernetEapProvider::CredentialChangeCallback callback0 =
      base::Bind(&EthernetEapProviderTest::Callback0,
                 base::Unretained(this));

  provider_.SetCredentialChangeCallback(device0, callback0);
  EXPECT_CALL(*this, Callback0());
  EXPECT_CALL(*this, Callback1()).Times(0);
  provider_.OnCredentialsChanged();

  scoped_refptr<MockEthernet> device1 = new MockEthernet(&control_,
                                                         &dispatcher_,
                                                         &metrics_,
                                                         &manager_,
                                                         "eth1",
                                                         "addr1",
                                                         1);
  EthernetEapProvider::CredentialChangeCallback callback1 =
      base::Bind(&EthernetEapProviderTest::Callback1,
                 base::Unretained(this));

  provider_.SetCredentialChangeCallback(device1, callback1);
  EXPECT_CALL(*this, Callback0());
  EXPECT_CALL(*this, Callback1());
  provider_.OnCredentialsChanged();

  provider_.SetCredentialChangeCallback(device1, callback0);
  EXPECT_CALL(*this, Callback0()).Times(2);
  EXPECT_CALL(*this, Callback1()).Times(0);
  provider_.OnCredentialsChanged();

  provider_.ClearCredentialChangeCallback(device0);
  EXPECT_CALL(*this, Callback0());
  EXPECT_CALL(*this, Callback1()).Times(0);
  provider_.OnCredentialsChanged();

  provider_.ClearCredentialChangeCallback(device1);
  EXPECT_CALL(*this, Callback0()).Times(0);
  EXPECT_CALL(*this, Callback1()).Times(0);
  provider_.OnCredentialsChanged();
}

}  // namespace shill
