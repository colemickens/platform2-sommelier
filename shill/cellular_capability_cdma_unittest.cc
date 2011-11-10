// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_cdma.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_modem_cdma_proxy.h"
#include "shill/nice_mock_control.h"

using testing::Return;

namespace shill {

class CellularCapabilityCDMATest : public testing::Test {
 public:
  CellularCapabilityCDMATest()
      : cellular_(new Cellular(&control_,
                               &dispatcher_,
                               NULL,
                               "",
                               "",
                               0,
                               Cellular::kTypeCDMA,
                               "",
                               "",
                               NULL)),
        proxy_(new MockModemCDMAProxy()),
        capability_(cellular_.get()) {}

  virtual ~CellularCapabilityCDMATest() {
    cellular_->service_ = NULL;
  }

 protected:
  static const char kMEID[];

  void SetRegistrationStateEVDO(uint32 state) {
    cellular_->cdma_.registration_state_evdo = state;
  }

  void SetRegistrationState1x(uint32 state) {
    cellular_->cdma_.registration_state_1x = state;
  }

  void SetProxy() {
    cellular_->set_modem_cdma_proxy(proxy_.release());
  }

  void SetService() {
    cellular_->service_ = new CellularService(
        &control_, &dispatcher_, NULL, cellular_);
  }

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  CellularRefPtr cellular_;
  scoped_ptr<MockModemCDMAProxy> proxy_;
  CellularCapabilityCDMA capability_;
};

const char CellularCapabilityCDMATest::kMEID[] = "D1234567EF8901";

TEST_F(CellularCapabilityCDMATest, GetIdentifiers) {
  EXPECT_CALL(*proxy_, MEID()).WillOnce(Return(kMEID));
  SetProxy();
  capability_.GetIdentifiers();
  EXPECT_EQ(kMEID, cellular_->meid());
  capability_.GetIdentifiers();
  EXPECT_EQ(kMEID, cellular_->meid());
}

TEST_F(CellularCapabilityCDMATest, GetNetworkTechnologyString) {
  EXPECT_EQ("", capability_.GetNetworkTechnologyString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(flimflam::kNetworkTechnologyEvdo,
            capability_.GetNetworkTechnologyString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(flimflam::kNetworkTechnology1Xrtt,
            capability_.GetNetworkTechnologyString());
}

TEST_F(CellularCapabilityCDMATest, GetRoamingStateString) {
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_.GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED);
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_.GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(flimflam::kRoamingStateHome, capability_.GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING);
  EXPECT_EQ(flimflam::kRoamingStateRoaming,
            capability_.GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED);
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_.GetRoamingStateString());
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(flimflam::kRoamingStateHome, capability_.GetRoamingStateString());
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING);
  EXPECT_EQ(flimflam::kRoamingStateRoaming,
            capability_.GetRoamingStateString());
}

TEST_F(CellularCapabilityCDMATest, GetSignalQuality) {
  const int kStrength = 90;
  EXPECT_CALL(*proxy_, GetSignalQuality()).WillOnce(Return(kStrength));
  SetProxy();
  SetService();
  EXPECT_EQ(0, cellular_->service()->strength());
  capability_.GetSignalQuality();
  EXPECT_EQ(kStrength, cellular_->service()->strength());
}

}  // namespace shill
