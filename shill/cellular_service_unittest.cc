// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_service.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/cellular_capability.h"
#include "shill/cellular_capability_cdma.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_metrics.h"
#include "shill/mock_profile.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using std::string;
using testing::_;
using testing::InSequence;
using testing::NiceMock;

namespace shill {

class CellularServiceTest : public testing::Test {
 public:
  CellularServiceTest()
      : device_(new Cellular(&control_,
                             NULL,
                             &metrics_,
                             NULL,
                             "usb0",
                             kAddress,
                             3,
                             Cellular::kTypeCDMA,
                             "",
                             "",
                             "",
                             NULL,
                             ProxyFactory::GetInstance())),
        service_(new CellularService(&control_, NULL, &metrics_, NULL,
                                     device_)),
        adaptor_(NULL) {}

  virtual ~CellularServiceTest() {
    adaptor_ = NULL;
  }

  virtual void SetUp() {
    adaptor_ =
        dynamic_cast<NiceMock<ServiceMockAdaptor> *>(service_->adaptor());
  }

  CellularCapabilityCDMA *GetCapabilityCDMA() {
    return dynamic_cast<CellularCapabilityCDMA *>(device_->capability_.get());
  }

 protected:
  static const char kAddress[];

  NiceMockControl control_;
  MockMetrics metrics_;
  CellularRefPtr device_;
  CellularServiceRefPtr service_;
  NiceMock<ServiceMockAdaptor> *adaptor_;  // Owned by |service_|.
};

const char CellularServiceTest::kAddress[] = "000102030405";

TEST_F(CellularServiceTest, Constructor) {
  EXPECT_TRUE(service_->connectable());
}

TEST_F(CellularServiceTest, SetNetworkTechnology) {
  EXPECT_CALL(*adaptor_, EmitStringChanged(flimflam::kNetworkTechnologyProperty,
                                           flimflam::kNetworkTechnologyUmts));
  EXPECT_TRUE(service_->network_technology().empty());
  service_->SetNetworkTechnology(flimflam::kNetworkTechnologyUmts);
  EXPECT_EQ(flimflam::kNetworkTechnologyUmts, service_->network_technology());
  service_->SetNetworkTechnology(flimflam::kNetworkTechnologyUmts);
}

TEST_F(CellularServiceTest, SetRoamingState) {
  EXPECT_CALL(*adaptor_, EmitStringChanged(flimflam::kRoamingStateProperty,
                                           flimflam::kRoamingStateHome));
  EXPECT_TRUE(service_->roaming_state().empty());
  service_->SetRoamingState(flimflam::kRoamingStateHome);
  EXPECT_EQ(flimflam::kRoamingStateHome, service_->roaming_state());
  service_->SetRoamingState(flimflam::kRoamingStateHome);
}

TEST_F(CellularServiceTest, FriendlyName) {
  static const char kCarrier[] = "Cellular Carrier";
  GetCapabilityCDMA()->carrier_ = kCarrier;
  service_ = new CellularService(&control_, NULL, &metrics_, NULL, device_);
  EXPECT_EQ(kCarrier, service_->friendly_name());
}

TEST_F(CellularServiceTest, SetStorageIdentifier) {
  EXPECT_EQ(string(flimflam::kTypeCellular) + "_" +
            kAddress + "_" + service_->friendly_name(),
            service_->GetStorageIdentifier());
  service_->SetStorageIdentifier("a b c");
  EXPECT_EQ("a_b_c", service_->GetStorageIdentifier());
}

TEST_F(CellularServiceTest, SetServingOperator) {
  EXPECT_CALL(*adaptor_,
              EmitStringmapChanged(flimflam::kServingOperatorProperty, _));
  static const char kCode[] = "123456";
  static const char kName[] = "Some Cellular Operator";
  Cellular::Operator oper;
  service_->SetServingOperator(oper);
  oper.SetCode(kCode);
  oper.SetName(kName);
  service_->SetServingOperator(oper);
  EXPECT_EQ(kCode, service_->serving_operator().GetCode());
  EXPECT_EQ(kName, service_->serving_operator().GetName());
  service_->SetServingOperator(oper);
}

TEST_F(CellularServiceTest, SetOLP) {
  EXPECT_CALL(*adaptor_,
              EmitStringmapChanged(flimflam::kPaymentPortalProperty, _));
  static const char kURL[] = "payment.url";
  static const char kMethod[] = "GET";
  CellularService::OLP olp;
  service_->SetOLP(olp);
  olp.SetURL(kURL);
  olp.SetMethod(kMethod);
  service_->SetOLP(olp);
  EXPECT_EQ(kURL, service_->olp().GetURL());
  EXPECT_EQ(kMethod, service_->olp().GetMethod());
  service_->SetOLP(olp);
}

TEST_F(CellularServiceTest, SetUsageURL) {
  static const char kUsageURL[] = "usage.url";
  EXPECT_CALL(*adaptor_, EmitStringChanged(flimflam::kUsageURLProperty,
                                           kUsageURL));
  EXPECT_TRUE(service_->usage_url().empty());
  service_->SetUsageURL(kUsageURL);
  EXPECT_EQ(kUsageURL, service_->usage_url());
  service_->SetUsageURL(kUsageURL);
}

TEST_F(CellularServiceTest, SetApn) {
  static const char kApn[] = "TheAPN";
  static const char kUsername[] = "commander.data";
  ProfileRefPtr profile(new NiceMock<MockProfile>(
      &control_, reinterpret_cast<Manager *>(NULL)));
  service_->set_profile(profile);
  Error error;
  Stringmap testapn;
  testapn[flimflam::kApnProperty] = kApn;
  testapn[flimflam::kApnUsernameProperty] = kUsername;
  {
    InSequence seq;
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(flimflam::kCellularLastGoodApnProperty,
                                     _));
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(flimflam::kCellularApnProperty, _));
  }
  service_->SetApn(testapn, &error);
  EXPECT_TRUE(error.IsSuccess());
  Stringmap resultapn = service_->GetApn(&error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(2, resultapn.size());
  Stringmap::const_iterator it = resultapn.find(flimflam::kApnProperty);
  EXPECT_TRUE(it != resultapn.end() && it->second == kApn);
  it = resultapn.find(flimflam::kApnUsernameProperty);
  EXPECT_TRUE(it != resultapn.end() && it->second == kUsername);
  EXPECT_FALSE(service_->GetUserSpecifiedApn() == NULL);
}

TEST_F(CellularServiceTest, ClearApn) {
  static const char kApn[] = "TheAPN";
  static const char kUsername[] = "commander.data";
  ProfileRefPtr profile(new NiceMock<MockProfile>(
      &control_, reinterpret_cast<Manager *>(NULL)));
  service_->set_profile(profile);
  Error error;
  // Set up an APN to make sure that it later gets cleared.
  Stringmap testapn;
  testapn[flimflam::kApnProperty] = kApn;
  testapn[flimflam::kApnUsernameProperty] = kUsername;
  {
    InSequence seq;
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(flimflam::kCellularLastGoodApnProperty,
                                     _));
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(flimflam::kCellularApnProperty, _));
  }
  service_->SetApn(testapn, &error);
  Stringmap resultapn = service_->GetApn(&error);
  ASSERT_TRUE(error.IsSuccess());
  ASSERT_EQ(2, service_->GetApn(&error).size());

  Stringmap emptyapn;
  EXPECT_CALL(*adaptor_,
              EmitStringmapChanged(flimflam::kCellularLastGoodApnProperty,
                                   _)).Times(0);
  EXPECT_CALL(*adaptor_,
              EmitStringmapChanged(flimflam::kCellularApnProperty, _)).Times(1);
  service_->SetApn(emptyapn, &error);
  EXPECT_TRUE(error.IsSuccess());
  resultapn = service_->GetApn(&error);
  EXPECT_TRUE(resultapn.empty());
  EXPECT_TRUE(service_->GetUserSpecifiedApn() == NULL);
}

TEST_F(CellularServiceTest, LastGoodApn) {
  static const char kApn[] = "TheAPN";
  static const char kUsername[] = "commander.data";
  ProfileRefPtr profile(new NiceMock<MockProfile>(
      &control_, reinterpret_cast<Manager *>(NULL)));
  service_->set_profile(profile);
  Stringmap testapn;
  testapn[flimflam::kApnProperty] = kApn;
  testapn[flimflam::kApnUsernameProperty] = kUsername;
  EXPECT_CALL(*adaptor_,
              EmitStringmapChanged(flimflam::kCellularLastGoodApnProperty, _));
  service_->SetLastGoodApn(testapn);
  Stringmap *resultapn = service_->GetLastGoodApn();
  EXPECT_FALSE(resultapn == NULL);
  EXPECT_EQ(2, resultapn->size());
  Stringmap::const_iterator it = resultapn->find(flimflam::kApnProperty);
  EXPECT_TRUE(it != resultapn->end() && it->second == kApn);
  it = resultapn->find(flimflam::kApnUsernameProperty);
  EXPECT_TRUE(it != resultapn->end() && it->second == kUsername);
  // Now set the user-specified APN, and check that LastGoodApn got
  // cleared.
  Stringmap userapn;
  userapn[flimflam::kApnProperty] = kApn;
  userapn[flimflam::kApnUsernameProperty] = kUsername;
  {
    InSequence seq;
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(flimflam::kCellularLastGoodApnProperty,
                                     _));
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(flimflam::kCellularApnProperty, _));
  }
  Error error;
  service_->SetApn(userapn, &error);
  EXPECT_TRUE(service_->GetLastGoodApn() == NULL);
}

}  // namespace shill
