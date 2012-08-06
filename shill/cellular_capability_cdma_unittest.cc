// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_cdma.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_modem_cdma_proxy.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using base::Bind;
using base::Unretained;
using std::string;
using testing::_;
using testing::Invoke;
using testing::Return;

namespace shill {

class CellularCapabilityCDMATest : public testing::Test {
 public:
  CellularCapabilityCDMATest()
      : manager_(&control_, &dispatcher_, &metrics_, &glib_),
        cellular_(new Cellular(&control_,
                               &dispatcher_,
                               &metrics_,
                               &manager_,
                               "",
                               "",
                               0,
                               Cellular::kTypeCDMA,
                               "",
                               "",
                               "",
                               NULL,
                               ProxyFactory::GetInstance())),
        proxy_(new MockModemCDMAProxy()),
        capability_(NULL) {}

  virtual ~CellularCapabilityCDMATest() {
    cellular_->service_ = NULL;
    capability_ = NULL;
  }

  virtual void SetUp() {
    capability_ =
        dynamic_cast<CellularCapabilityCDMA *>(cellular_->capability_.get());
  }

  void InvokeActivate(const string &carrier, Error *error,
                      const ActivationResultCallback &callback,
                      int timeout) {
    callback.Run(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR, Error());
  }
  void InvokeActivateError(const string &carrier, Error *error,
                           const ActivationResultCallback &callback,
                           int timeout) {
    callback.Run(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_SIGNAL, Error());
  }
  void InvokeGetSignalQuality(Error *error,
                              const SignalQualityCallback &callback,
                              int timeout) {
    callback.Run(kStrength, Error());
  }
  void InvokeGetRegistrationState(Error *error,
                                  const RegistrationStateCallback &callback,
                                  int timeout) {
    callback.Run(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED,
                 MM_MODEM_CDMA_REGISTRATION_STATE_HOME,
                 Error());
  }

  MOCK_METHOD1(TestCallback, void(const Error &error));

 protected:
  static const char kMEID[];
  static const char kTestCarrier[];
  static const unsigned int kStrength;

  void SetRegistrationStateEVDO(uint32 state) {
    capability_->registration_state_evdo_ = state;
  }

  void SetRegistrationState1x(uint32 state) {
    capability_->registration_state_1x_ = state;
  }

  void SetProxy() {
    capability_->proxy_.reset(proxy_.release());
  }

  void SetService() {
    cellular_->service_ = new CellularService(
        &control_, &dispatcher_, &metrics_, &manager_, cellular_);
  }

  void SetDeviceState(Cellular::State state) {
    cellular_->state_ = state;
  }

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockGLib glib_;
  MockManager manager_;
  MockMetrics metrics_;
  CellularRefPtr cellular_;
  scoped_ptr<MockModemCDMAProxy> proxy_;
  CellularCapabilityCDMA *capability_;  // Owned by |cellular_|.
};

const char CellularCapabilityCDMATest::kMEID[] = "D1234567EF8901";
const char CellularCapabilityCDMATest::kTestCarrier[] = "The Cellular Carrier";
const unsigned int CellularCapabilityCDMATest::kStrength = 90;

TEST_F(CellularCapabilityCDMATest, PropertyStore) {
  EXPECT_TRUE(cellular_->store().Contains(flimflam::kPRLVersionProperty));
}

TEST_F(CellularCapabilityCDMATest, Activate) {
  SetDeviceState(Cellular::kStateEnabled);
  EXPECT_CALL(*proxy_, Activate(kTestCarrier, _, _,
                                CellularCapability::kTimeoutActivate))
      .WillOnce(Invoke(this,
                       &CellularCapabilityCDMATest::InvokeActivate));
  EXPECT_CALL(*this, TestCallback(_));
  SetProxy();
  SetService();
  capability_->Activate(kTestCarrier, NULL,
      Bind(&CellularCapabilityCDMATest::TestCallback, Unretained(this)));
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING,
            capability_->activation_state());
  EXPECT_EQ(flimflam::kActivationStateActivating,
            cellular_->service()->activation_state());
  EXPECT_EQ("", cellular_->service()->error());
}

TEST_F(CellularCapabilityCDMATest, ActivateError) {
  SetDeviceState(Cellular::kStateEnabled);
  EXPECT_CALL(*proxy_, Activate(kTestCarrier, _, _,
                                CellularCapability::kTimeoutActivate))
      .WillOnce(Invoke(this,
                       &CellularCapabilityCDMATest::InvokeActivateError));
  EXPECT_CALL(*this, TestCallback(_));
  SetProxy();
  SetService();
  capability_->Activate(kTestCarrier, NULL,
      Bind(&CellularCapabilityCDMATest::TestCallback, Unretained(this)));
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED,
            capability_->activation_state());
  EXPECT_EQ(flimflam::kActivationStateNotActivated,
            cellular_->service()->activation_state());
  EXPECT_EQ(flimflam::kErrorActivationFailed,
            cellular_->service()->error());
}

TEST_F(CellularCapabilityCDMATest, GetActivationStateString) {
  EXPECT_EQ(flimflam::kActivationStateActivated,
            CellularCapabilityCDMA::GetActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED));
  EXPECT_EQ(flimflam::kActivationStateActivating,
            CellularCapabilityCDMA::GetActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING));
  EXPECT_EQ(flimflam::kActivationStateNotActivated,
            CellularCapabilityCDMA::GetActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED));
  EXPECT_EQ(flimflam::kActivationStatePartiallyActivated,
            CellularCapabilityCDMA::GetActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_PARTIALLY_ACTIVATED));
  EXPECT_EQ(flimflam::kActivationStateUnknown,
            CellularCapabilityCDMA::GetActivationStateString(123));
}

TEST_F(CellularCapabilityCDMATest, GetActivationErrorString) {
  EXPECT_EQ(flimflam::kErrorNeedEvdo,
            CellularCapabilityCDMA::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_WRONG_RADIO_INTERFACE));
  EXPECT_EQ(flimflam::kErrorNeedHomeNetwork,
            CellularCapabilityCDMA::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_ROAMING));
  EXPECT_EQ(flimflam::kErrorOtaspFailed,
            CellularCapabilityCDMA::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_COULD_NOT_CONNECT));
  EXPECT_EQ(flimflam::kErrorOtaspFailed,
            CellularCapabilityCDMA::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_SECURITY_AUTHENTICATION_FAILED));
  EXPECT_EQ(flimflam::kErrorOtaspFailed,
            CellularCapabilityCDMA::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_PROVISIONING_FAILED));
  EXPECT_EQ("",
            CellularCapabilityCDMA::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR));
  EXPECT_EQ(flimflam::kErrorActivationFailed,
            CellularCapabilityCDMA::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_NO_SIGNAL));
  EXPECT_EQ(flimflam::kErrorActivationFailed,
            CellularCapabilityCDMA::GetActivationErrorString(1234));
}

TEST_F(CellularCapabilityCDMATest, IsRegisteredEVDO) {
  EXPECT_FALSE(capability_->IsRegistered());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
  EXPECT_FALSE(capability_->IsRegistered());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED);
  EXPECT_TRUE(capability_->IsRegistered());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_TRUE(capability_->IsRegistered());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING);
  EXPECT_TRUE(capability_->IsRegistered());
}

TEST_F(CellularCapabilityCDMATest, IsRegistered1x) {
  EXPECT_FALSE(capability_->IsRegistered());
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
  EXPECT_FALSE(capability_->IsRegistered());
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED);
  EXPECT_TRUE(capability_->IsRegistered());
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_TRUE(capability_->IsRegistered());
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING);
  EXPECT_TRUE(capability_->IsRegistered());
}

TEST_F(CellularCapabilityCDMATest, GetNetworkTechnologyString) {
  EXPECT_EQ("", capability_->GetNetworkTechnologyString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(flimflam::kNetworkTechnologyEvdo,
            capability_->GetNetworkTechnologyString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(flimflam::kNetworkTechnology1Xrtt,
            capability_->GetNetworkTechnologyString());
}

TEST_F(CellularCapabilityCDMATest, GetRoamingStateString) {
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_->GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED);
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_->GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(flimflam::kRoamingStateHome, capability_->GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING);
  EXPECT_EQ(flimflam::kRoamingStateRoaming,
            capability_->GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED);
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_->GetRoamingStateString());
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(flimflam::kRoamingStateHome, capability_->GetRoamingStateString());
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING);
  EXPECT_EQ(flimflam::kRoamingStateRoaming,
            capability_->GetRoamingStateString());
}

TEST_F(CellularCapabilityCDMATest, GetSignalQuality) {
  EXPECT_CALL(*proxy_,
              GetSignalQuality(NULL, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this,
                       &CellularCapabilityCDMATest::InvokeGetSignalQuality));
  SetProxy();
  SetService();
  EXPECT_EQ(0, cellular_->service()->strength());
  capability_->GetSignalQuality();
  EXPECT_EQ(kStrength, cellular_->service()->strength());
}

TEST_F(CellularCapabilityCDMATest, GetRegistrationState) {
  EXPECT_FALSE(cellular_->service().get());
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            capability_->registration_state_1x());
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            capability_->registration_state_evdo());
  EXPECT_CALL(*proxy_,
              GetRegistrationState(NULL, _,
                                   CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(
          this,
          &CellularCapabilityCDMATest::InvokeGetRegistrationState));
  SetProxy();
  cellular_->state_ = Cellular::kStateEnabled;
  EXPECT_CALL(manager_, RegisterService(_));
  capability_->GetRegistrationState();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED,
            capability_->registration_state_1x());
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_HOME,
            capability_->registration_state_evdo());
  EXPECT_TRUE(cellular_->service().get());
}

TEST_F(CellularCapabilityCDMATest, CreateFriendlyServiceName) {
  CellularCapabilityCDMA::friendly_service_name_id_ = 0;
  EXPECT_EQ("CDMANetwork0", capability_->CreateFriendlyServiceName());
  EXPECT_EQ("CDMANetwork1", capability_->CreateFriendlyServiceName());
  static const char kTestCarrier[] = "A Carrier";
  capability_->carrier_ = kTestCarrier;
  EXPECT_EQ(kTestCarrier, capability_->CreateFriendlyServiceName());
}

}  // namespace shill
