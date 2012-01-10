// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_service.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"

using testing::NiceMock;

namespace shill {

class CellularServiceTest : public testing::Test {
 public:
  CellularServiceTest()
      : device_(new Cellular(&control_,
                             NULL,
                             NULL,
                             "usb0",
                             "00:01:02:03:04:05",
                             3,
                             Cellular::kTypeGSM,
                             "",
                             "",
                             NULL)),
        service_(new CellularService(&control_, NULL, NULL, device_)),
        adaptor_(NULL) {}

  virtual ~CellularServiceTest() {
    adaptor_ = NULL;
  }

  virtual void SetUp() {
    adaptor_ =
        dynamic_cast<NiceMock<ServiceMockAdaptor> *>(service_->adaptor());
  }

 protected:
  NiceMockControl control_;
  CellularRefPtr device_;
  CellularServiceRefPtr service_;
  NiceMock<ServiceMockAdaptor> *adaptor_;  // Owned by |service_|.
};

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
  device_->carrier_ = kCarrier;
  service_ = new CellularService(&control_, NULL, NULL, device_);
  EXPECT_EQ(kCarrier, service_->friendly_name());
}

}  // namespace shill
