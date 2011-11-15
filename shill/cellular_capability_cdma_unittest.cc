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

using testing::_;
using testing::Return;
using testing::SetArgumentPointee;

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

  virtual void TearDown() {
    CellularCapability *capability = cellular_->capability_.release();
    ASSERT_TRUE(capability == NULL || capability == &capability_);
  }

 protected:
  static const char kMEID[];
  static const char kTestCarrier[];

  void SetRegistrationStateEVDO(uint32 state) {
    capability_.registration_state_evdo_ = state;
  }

  void SetRegistrationState1x(uint32 state) {
    capability_.registration_state_1x_ = state;
  }

  void SetProxy() {
    capability_.proxy_.reset(proxy_.release());
  }

  void SetService() {
    cellular_->service_ = new CellularService(
        &control_, &dispatcher_, NULL, cellular_);
  }

  void SetCapability() {
    ASSERT_FALSE(cellular_->capability_.get());
    cellular_->capability_.reset(&capability_);
  }

  void SetDeviceState(Cellular::State state) {
    cellular_->state_ = state;
  }

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  CellularRefPtr cellular_;
  scoped_ptr<MockModemCDMAProxy> proxy_;
  CellularCapabilityCDMA capability_;
};

const char CellularCapabilityCDMATest::kMEID[] = "D1234567EF8901";
const char CellularCapabilityCDMATest::kTestCarrier[] = "The Cellular Carrier";

TEST_F(CellularCapabilityCDMATest, Activate) {
  Error error;
  SetDeviceState(Cellular::kStateEnabled);
  EXPECT_CALL(*proxy_, Activate(kTestCarrier))
      .WillOnce(Return(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR));
  capability_.Activate(kTestCarrier, &error);
  EXPECT_TRUE(error.IsSuccess());
  SetProxy();
  SetService();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING,
            cellular_->cdma_activation_state());
  EXPECT_EQ(flimflam::kActivationStateActivating,
            cellular_->service()->activation_state());
  EXPECT_EQ("", cellular_->service()->error());
}

TEST_F(CellularCapabilityCDMATest, ActivateError) {
  Error error;
  capability_.Activate(kTestCarrier, &error);
  EXPECT_EQ(Error::kInvalidArguments, error.type());

  error.Reset();
  SetDeviceState(Cellular::kStateRegistered);
  EXPECT_CALL(*proxy_, Activate(kTestCarrier))
      .WillOnce(Return(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_SIGNAL));
  capability_.Activate(kTestCarrier, &error);
  EXPECT_TRUE(error.IsSuccess());
  SetProxy();
  SetService();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED,
            cellular_->cdma_activation_state());
  EXPECT_EQ(flimflam::kActivationStateNotActivated,
            cellular_->service()->activation_state());
  EXPECT_EQ(flimflam::kErrorActivationFailed,
            cellular_->service()->error());
}

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

TEST_F(CellularCapabilityCDMATest, GetRegistrationState) {
  EXPECT_FALSE(cellular_->service().get());
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            capability_.registration_state_1x());
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            capability_.registration_state_evdo());
  EXPECT_CALL(*proxy_, GetRegistrationState(_, _))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED),
          SetArgumentPointee<1>(MM_MODEM_CDMA_REGISTRATION_STATE_HOME)));
  EXPECT_CALL(*proxy_, GetSignalQuality()).WillOnce(Return(90));
  SetProxy();
  capability_.GetRegistrationState();
  SetCapability();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED,
            capability_.registration_state_1x());
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_HOME,
            capability_.registration_state_evdo());
  ASSERT_TRUE(cellular_->service().get());
}

}  // namespace shill
