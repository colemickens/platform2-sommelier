// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_service.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/cellular_capability.h"
#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_metrics.h"

using std::string;
using testing::_;
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
                             Cellular::kTypeGSM,
                             "",
                             "",
                             NULL)),
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

 protected:
  static const char kAddress[];

  NiceMockControl control_;
  MockMetrics metrics_;
  CellularRefPtr device_;
  CellularServiceRefPtr service_;
  NiceMock<ServiceMockAdaptor> *adaptor_;  // Owned by |service_|.
};

const char CellularServiceTest::kAddress[] = "000102030405";

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
  device_->capability_->carrier_ = kCarrier;
  service_ = new CellularService(&control_, NULL, &metrics_, NULL, device_);
  EXPECT_EQ(kCarrier, service_->friendly_name());
}

TEST_F(CellularServiceTest, SetStorageIdentifier) {
  EXPECT_EQ(string("cellular_") + kAddress + "_" + service_->friendly_name(),
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

}  // namespace shill
