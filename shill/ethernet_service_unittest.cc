// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet_service.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_adaptors.h"
#include "shill/mock_ethernet.h"
#include "shill/mock_manager.h"
#include "shill/mock_store.h"
#include "shill/property_store_unittest.h"
#include "shill/refptr_types.h"
#include "shill/service_property_change_test.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace shill {

class EthernetServiceTest : public PropertyStoreTest {
 public:
  EthernetServiceTest()
      : mock_manager_(control_interface(), dispatcher(), metrics(), glib()),
        ethernet_(
            new NiceMock<MockEthernet>(control_interface(),
                                       dispatcher(),
                                       metrics(),
                                       &mock_manager_,
                                       "ethernet",
                                       fake_mac,
                                       0)),
        service_(
            new EthernetService(control_interface(),
                                dispatcher(),
                                metrics(),
                                &mock_manager_,
                                ethernet_)) {}
  virtual ~EthernetServiceTest() {}

 protected:
  static const char fake_mac[];

  bool GetAutoConnect() {
    return service_->GetAutoConnect(NULL);
  }

  bool SetAutoConnect(const bool connect, Error *error) {
    return service_->SetAutoConnectFull(connect, error);
  }

  ServiceMockAdaptor *GetAdaptor() {
    return dynamic_cast<ServiceMockAdaptor *>(service_->adaptor());
  }

  MockManager mock_manager_;
  scoped_refptr<MockEthernet> ethernet_;
  EthernetServiceRefPtr service_;
};

// static
const char EthernetServiceTest::fake_mac[] = "AaBBcCDDeeFF";

TEST_F(EthernetServiceTest, AutoConnect) {
  EXPECT_TRUE(service_->IsAutoConnectByDefault());
  EXPECT_TRUE(GetAutoConnect());
  {
    Error error;
    SetAutoConnect(false, &error);
    EXPECT_FALSE(error.IsSuccess());
  }
  EXPECT_TRUE(GetAutoConnect());
  {
    Error error;
    SetAutoConnect(true, &error);
    EXPECT_TRUE(error.IsSuccess());
  }
  EXPECT_TRUE(GetAutoConnect());
}

TEST_F(EthernetServiceTest, ConnectDisconnectDelegation) {
  EXPECT_CALL(*ethernet_, ConnectTo(service_.get()));
  service_->AutoConnect();
  EXPECT_CALL(*ethernet_, DisconnectFrom(service_.get()));
  Error error;
  service_->Disconnect(&error);
}

TEST_F(EthernetServiceTest, PropertyChanges) {
  TestCommonPropertyChanges(service_, GetAdaptor());
}

// Custom property setters should return false, and make no changes, if
// the new value is the same as the old value.
TEST_F(EthernetServiceTest, CustomSetterNoopChange) {
  TestCustomSetterNoopChange(service_, &mock_manager_);
}

TEST_F(EthernetServiceTest, LoadAutoConnect) {
  // Make sure when we try to load an Ethernet service, it sets AutoConnect
  // to be true even if the property is not found.
  NiceMock<MockStore> mock_store;
  EXPECT_CALL(mock_store, ContainsGroup(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_store, GetBool(_, _, _)).WillRepeatedly(Return(false));
  EXPECT_TRUE(service_->Load(&mock_store));
  EXPECT_TRUE(GetAutoConnect());
}

TEST_F(EthernetServiceTest, GetTethering) {
  EXPECT_CALL(*ethernet_, IsConnectedViaTether())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_EQ(kTetheringConfirmedState, service_->GetTethering(NULL));
  EXPECT_EQ(kTetheringNotDetectedState, service_->GetTethering(NULL));
}

}  // namespace shill
