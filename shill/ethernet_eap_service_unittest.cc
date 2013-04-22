// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet_eap_service.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/mock_control.h"
#include "shill/mock_ethernet_eap_provider.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/technology.h"

using testing::Return;

namespace shill {

class EthernetEapServiceTest : public testing::Test {
 public:
  EthernetEapServiceTest()
      : metrics_(&dispatcher_),
        manager_(&control_, &dispatcher_, &metrics_, NULL),
        service_(new EthernetEapService(&control_,
                                        &dispatcher_,
                                        &metrics_,
                                        &manager_)) {}
  virtual ~EthernetEapServiceTest() {}

 protected:
  MockControl control_;
  MockEventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockManager manager_;
  MockEthernetEapProvider provider_;
  scoped_refptr<EthernetEapService> service_;
};

TEST_F(EthernetEapServiceTest, MethodOverrides) {
  EXPECT_EQ("/", service_->GetDeviceRpcId(NULL));
  EXPECT_EQ("etherneteap_all", service_->GetStorageIdentifier());
  EXPECT_EQ(Technology::kEthernetEap, service_->technology());
  EXPECT_TRUE(service_->Is8021x());
  EXPECT_FALSE(service_->IsVisible());
}

TEST_F(EthernetEapServiceTest, OnEapCredentialsChanged) {
  EXPECT_CALL(manager_, ethernet_eap_provider()).WillOnce(Return(&provider_));
  EXPECT_CALL(provider_, OnCredentialsChanged());
  service_->OnEapCredentialsChanged();
}

TEST_F(EthernetEapServiceTest, OnEapCredentialPropertyChanged) {
  EXPECT_CALL(manager_, ethernet_eap_provider()).WillOnce(Return(&provider_));
  EXPECT_CALL(provider_, OnCredentialsChanged());
  service_->OnPropertyChanged(flimflam::kEapPasswordProperty);
}

TEST_F(EthernetEapServiceTest, Unload) {
  EXPECT_CALL(manager_, ethernet_eap_provider()).WillOnce(Return(&provider_));
  EXPECT_CALL(provider_, OnCredentialsChanged());
  EXPECT_FALSE(service_->Unload());
}

}  // namespace shill
