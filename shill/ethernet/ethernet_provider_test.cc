// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet/ethernet_provider.h"

#include <base/bind.h>
#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/ethernet/mock_ethernet.h"
#include "shill/key_value_store.h"
#include "shill/mock_control.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_profile.h"
#include "shill/mock_store.h"
#include "shill/test_event_dispatcher.h"

using testing::_;
using testing::DoAll;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;

namespace shill {

class EthernetProviderTest : public testing::Test {
 public:
  EthernetProviderTest()
      : manager_(&control_, &dispatcher_, &metrics_),
        profile_(new MockProfile(&manager_, "")),
        provider_(&manager_) {}
  ~EthernetProviderTest() override = default;

 protected:
  using MockProfileRefPtr = scoped_refptr<MockProfile>;

  MockControl control_;
  EventDispatcherForTest dispatcher_;
  MockMetrics metrics_;
  MockManager manager_;
  MockProfileRefPtr profile_;
  EthernetProvider provider_;
};

TEST_F(EthernetProviderTest, Construct) {
  EXPECT_EQ(ServiceRefPtr(), provider_.service());
}

TEST_F(EthernetProviderTest, StartAndStop) {
  ServiceRefPtr service;
  ProfileRefPtr profile = profile_.get();
  EXPECT_CALL(manager_, ActiveProfile()).WillOnce(ReturnRef(profile));
  EXPECT_CALL(*profile_, ConfigureService(_))
      .WillOnce(DoAll(SaveArg<0>(&service), Return(true)));
  provider_.Start();
  EXPECT_NE(ServiceRefPtr(), provider_.service());
  EXPECT_EQ(service, provider_.service());

  EXPECT_CALL(manager_, ActiveProfile()).WillOnce(ReturnRef(profile));
  EXPECT_CALL(*profile_, UpdateService(_));
  provider_.Stop();
  EXPECT_EQ(service, provider_.service());

  // Provider re-uses the same service on restart.
  provider_.Start();
  Mock::VerifyAndClearExpectations(&manager_);
}

TEST_F(EthernetProviderTest, ServiceConstructors) {
  ServiceRefPtr service;
  ProfileRefPtr profile = profile_.get();
  EXPECT_CALL(manager_, ActiveProfile()).WillOnce(ReturnRef(profile));
  EXPECT_CALL(*profile_, ConfigureService(_))
      .WillOnce(DoAll(SaveArg<0>(&service), Return(true)));
  provider_.Start();
  KeyValueStore args;
  args.Set<std::string>(kTypeProperty, kTypeEthernet);
  {
    Error error;
    EXPECT_EQ(service, provider_.GetService(args, &error));
    EXPECT_TRUE(error.IsSuccess());
  }
  {
    Error error;
    EXPECT_EQ(service, provider_.FindSimilarService(args, &error));
    EXPECT_TRUE(error.IsSuccess());
  }
  {
    Error error;
    Mock::VerifyAndClearExpectations(&manager_);
    EXPECT_CALL(manager_, RegisterService(_)).Times(0);
    ServiceRefPtr temp_service = provider_.CreateTemporaryService(args, &error);
    EXPECT_TRUE(error.IsSuccess());
    // Returned service should be non-NULL but not the provider's own service.
    EXPECT_NE(ServiceRefPtr(), temp_service);
    EXPECT_NE(service, temp_service);
  }
}

TEST_F(EthernetProviderTest, GenericServiceCreation) {
  ServiceRefPtr service;
  ProfileRefPtr profile = profile_.get();
  EXPECT_CALL(manager_, ActiveProfile()).WillOnce(ReturnRef(profile));
  EXPECT_CALL(*profile_, ConfigureService(_))
      .WillOnce(DoAll(SaveArg<0>(&service), Return(true)));
  provider_.Start();
  EXPECT_NE(ServiceRefPtr(), provider_.service());
  EXPECT_EQ(service, provider_.service());
  EXPECT_EQ(service->GetStorageIdentifier(), "ethernet_any");

  EXPECT_CALL(manager_, ActiveProfile()).WillOnce(ReturnRef(profile));
  EXPECT_CALL(*profile_, UpdateService(_));
  provider_.Stop();
  EXPECT_EQ(service, provider_.service());
}

}  // namespace shill
