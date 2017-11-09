//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/cellular/cellular_capability_cdma.h"

#include <utility>

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>

#include "shill/cellular/cellular.h"
#include "shill/cellular/cellular_service.h"
#include "shill/cellular/mock_cellular.h"
#include "shill/cellular/mock_modem_cdma_proxy.h"
#include "shill/cellular/mock_modem_info.h"
#include "shill/cellular/mock_modem_proxy.h"
#include "shill/error.h"
#include "shill/mock_adaptors.h"
#include "shill/test_event_dispatcher.h"

using base::Bind;
using base::Unretained;
using std::string;
using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::StrEq;

namespace shill {

class CellularCapabilityCdmaTest : public testing::Test {
 public:
  CellularCapabilityCdmaTest()
      : modem_info_(nullptr, &dispatcher_, nullptr, nullptr),
        cellular_(new MockCellular(&modem_info_,
                                   "",
                                   "",
                                   0,
                                   Cellular::kTypeCdma,
                                   "",
                                   "")),
        classic_proxy_(new MockModemProxy()),
        proxy_(new MockModemCdmaProxy()),
        capability_(nullptr) {
    modem_info_.metrics()->RegisterDevice(cellular_->interface_index(),
                                          Technology::kCellular);
  }

  ~CellularCapabilityCdmaTest() override {
    cellular_->service_ = nullptr;
    capability_ = nullptr;
  }

  void SetUp() override {
    capability_ =
        static_cast<CellularCapabilityCdma*>(cellular_->capability_.get());
  }

  void InvokeActivate(const string& carrier, Error* error,
                      const ActivationResultCallback& callback,
                      int timeout) {
    callback.Run(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR, Error());
  }
  void InvokeActivateError(const string& carrier, Error* error,
                           const ActivationResultCallback& callback,
                           int timeout) {
    callback.Run(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_SIGNAL, Error());
  }
  void InvokeDisconnect(Error* error,
                        const ResultCallback& callback,
                        int timeout) {
    callback.Run(Error());
  }
  void InvokeDisconnectError(Error* error,
                             const ResultCallback& callback,
                             int timeout) {
    Error err(Error::kOperationFailed);
    callback.Run(err);
  }
  void InvokeGetSignalQuality(Error* error,
                              const SignalQualityCallback& callback,
                              int timeout) {
    callback.Run(kStrength, Error());
  }
  void InvokeGetRegistrationState(Error* error,
                                  const RegistrationStateCallback& callback,
                                  int timeout) {
    callback.Run(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED,
                 MM_MODEM_CDMA_REGISTRATION_STATE_HOME,
                 Error());
  }

  MOCK_METHOD1(TestCallback, void(const Error& error));

 protected:
  static const char kMEID[];
  static const char kTestCarrier[];
  static const unsigned int kStrength;

  bool IsActivationStarting() const {
    return capability_->activation_starting_;
  }

  void SetRegistrationStateEVDO(uint32_t state) {
    capability_->registration_state_evdo_ = state;
  }

  void SetRegistrationState1x(uint32_t state) {
    capability_->registration_state_1x_ = state;
  }

  void SetProxy() {
    capability_->proxy_ = std::move(proxy_);
    capability_->CellularCapabilityClassic::proxy_ = std::move(classic_proxy_);
  }

  void SetService() {
    cellular_->service_ = new CellularService(&modem_info_, cellular_);
  }

  void SetDeviceState(Cellular::State state) {
    cellular_->state_ = state;
  }

  EventDispatcherForTest dispatcher_;
  MockModemInfo modem_info_;
  scoped_refptr<MockCellular> cellular_;
  std::unique_ptr<MockModemProxy> classic_proxy_;
  std::unique_ptr<MockModemCdmaProxy> proxy_;
  CellularCapabilityCdma* capability_;  // Owned by |cellular_|.
};

const char CellularCapabilityCdmaTest::kMEID[] = "D1234567EF8901";
const char CellularCapabilityCdmaTest::kTestCarrier[] = "The Cellular Carrier";
const unsigned int CellularCapabilityCdmaTest::kStrength = 90;

TEST_F(CellularCapabilityCdmaTest, PropertyStore) {
  EXPECT_TRUE(cellular_->store().Contains(kPRLVersionProperty));
}

TEST_F(CellularCapabilityCdmaTest, Activate) {
  SetDeviceState(Cellular::kStateEnabled);
  EXPECT_CALL(*proxy_, Activate(kTestCarrier, _, _,
                                CellularCapability::kTimeoutActivate))
      .WillOnce(Invoke(this,
                       &CellularCapabilityCdmaTest::InvokeActivate));
  EXPECT_CALL(*this, TestCallback(_));
  SetProxy();
  SetService();
  capability_->Activate(kTestCarrier, nullptr,
      Bind(&CellularCapabilityCdmaTest::TestCallback, Unretained(this)));
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING,
            capability_->activation_state());
  EXPECT_EQ(kActivationStateActivating,
            cellular_->service()->activation_state());
  EXPECT_EQ("", cellular_->service()->error());
}

TEST_F(CellularCapabilityCdmaTest, ActivateWhileConnected) {
  SetDeviceState(Cellular::kStateConnected);
  {
    InSequence dummy;

    EXPECT_CALL(*cellular_, Disconnect(_, StrEq("Activate")));
    EXPECT_CALL(*proxy_, Activate(kTestCarrier, _, _,
                                  CellularCapability::kTimeoutActivate))
        .WillOnce(Invoke(this,
                         &CellularCapabilityCdmaTest::InvokeActivate));
    EXPECT_CALL(*this, TestCallback(_));
  }
  SetProxy();
  SetService();
  Error error;
  capability_->Activate(kTestCarrier, &error,
      Bind(&CellularCapabilityCdmaTest::TestCallback, Unretained(this)));
  // So now we should be "activating" while we wait for a disconnect.
  EXPECT_TRUE(IsActivationStarting());
  EXPECT_TRUE(capability_->IsActivating());
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED,
            capability_->activation_state());
  // Simulate a disconnect.
  SetDeviceState(Cellular::kStateRegistered);
  capability_->DisconnectCleanup();
  // Now the modem is actually activating.
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING,
            capability_->activation_state());
  EXPECT_EQ(kActivationStateActivating,
            cellular_->service()->activation_state());
  EXPECT_EQ("", cellular_->service()->error());
  EXPECT_FALSE(IsActivationStarting());
  EXPECT_TRUE(capability_->IsActivating());
}

TEST_F(CellularCapabilityCdmaTest, ActivateWhileConnectedButFail) {
  SetDeviceState(Cellular::kStateConnected);
  {
    InSequence dummy;

    EXPECT_CALL(*cellular_, Disconnect(_, StrEq("Activate")));
    EXPECT_CALL(*proxy_, Activate(kTestCarrier, _, _,
                                  CellularCapability::kTimeoutActivate))
        .Times(0);
  }
  SetProxy();
  SetService();
  Error error;
  capability_->Activate(kTestCarrier, &error,
      Bind(&CellularCapabilityCdmaTest::TestCallback, Unretained(this)));
  // So now we should be "activating" while we wait for a disconnect.
  EXPECT_TRUE(IsActivationStarting());
  EXPECT_TRUE(capability_->IsActivating());
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED,
            capability_->activation_state());
  // Similulate a failed disconnect (the modem is still connected!).
  capability_->DisconnectCleanup();
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED,
            capability_->activation_state());
  EXPECT_EQ(kActivationStateNotActivated,
            cellular_->service()->activation_state());
  EXPECT_EQ(kErrorActivationFailed, cellular_->service()->error());
  EXPECT_FALSE(IsActivationStarting());
  EXPECT_FALSE(capability_->IsActivating());
}

TEST_F(CellularCapabilityCdmaTest, ActivateError) {
  SetDeviceState(Cellular::kStateEnabled);
  EXPECT_CALL(*proxy_, Activate(kTestCarrier, _, _,
                                CellularCapability::kTimeoutActivate))
      .WillOnce(Invoke(this,
                       &CellularCapabilityCdmaTest::InvokeActivateError));
  EXPECT_CALL(*this, TestCallback(_));
  SetProxy();
  SetService();
  capability_->Activate(kTestCarrier, nullptr,
      Bind(&CellularCapabilityCdmaTest::TestCallback, Unretained(this)));
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED,
            capability_->activation_state());
  EXPECT_EQ(kActivationStateNotActivated,
            cellular_->service()->activation_state());
  EXPECT_EQ(kErrorActivationFailed,
            cellular_->service()->error());
}

TEST_F(CellularCapabilityCdmaTest, GetActivationStateString) {
  EXPECT_EQ(kActivationStateActivated,
            CellularCapabilityCdma::GetActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED));
  EXPECT_EQ(kActivationStateActivating,
            CellularCapabilityCdma::GetActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING));
  EXPECT_EQ(kActivationStateNotActivated,
            CellularCapabilityCdma::GetActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED));
  EXPECT_EQ(kActivationStatePartiallyActivated,
            CellularCapabilityCdma::GetActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_PARTIALLY_ACTIVATED));
  EXPECT_EQ(kActivationStateUnknown,
            CellularCapabilityCdma::GetActivationStateString(123));
}

TEST_F(CellularCapabilityCdmaTest, GetActivationErrorString) {
  EXPECT_EQ(kErrorNeedEvdo,
            CellularCapabilityCdma::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_WRONG_RADIO_INTERFACE));
  EXPECT_EQ(kErrorNeedHomeNetwork,
            CellularCapabilityCdma::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_ROAMING));
  EXPECT_EQ(kErrorOtaspFailed,
            CellularCapabilityCdma::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_COULD_NOT_CONNECT));
  EXPECT_EQ(kErrorOtaspFailed,
            CellularCapabilityCdma::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_SECURITY_AUTHENTICATION_FAILED));
  EXPECT_EQ(kErrorOtaspFailed,
            CellularCapabilityCdma::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_PROVISIONING_FAILED));
  EXPECT_EQ("",
            CellularCapabilityCdma::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR));
  EXPECT_EQ(kErrorActivationFailed,
            CellularCapabilityCdma::GetActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_NO_SIGNAL));
  EXPECT_EQ(kErrorActivationFailed,
            CellularCapabilityCdma::GetActivationErrorString(1234));
}

TEST_F(CellularCapabilityCdmaTest, IsRegisteredEVDO) {
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

TEST_F(CellularCapabilityCdmaTest, IsRegistered1x) {
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

TEST_F(CellularCapabilityCdmaTest, GetNetworkTechnologyString) {
  EXPECT_EQ("", capability_->GetNetworkTechnologyString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(kNetworkTechnologyEvdo,
            capability_->GetNetworkTechnologyString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(kNetworkTechnology1Xrtt,
            capability_->GetNetworkTechnologyString());
}

TEST_F(CellularCapabilityCdmaTest, GetRoamingStateString) {
  EXPECT_EQ(kRoamingStateUnknown,
            capability_->GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED);
  EXPECT_EQ(kRoamingStateUnknown,
            capability_->GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(kRoamingStateHome, capability_->GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING);
  EXPECT_EQ(kRoamingStateRoaming,
            capability_->GetRoamingStateString());
  SetRegistrationStateEVDO(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED);
  EXPECT_EQ(kRoamingStateUnknown,
            capability_->GetRoamingStateString());
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_HOME);
  EXPECT_EQ(kRoamingStateHome, capability_->GetRoamingStateString());
  SetRegistrationState1x(MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING);
  EXPECT_EQ(kRoamingStateRoaming,
            capability_->GetRoamingStateString());
}

TEST_F(CellularCapabilityCdmaTest, GetSignalQuality) {
  EXPECT_CALL(*proxy_,
              GetSignalQuality(nullptr, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this,
                       &CellularCapabilityCdmaTest::InvokeGetSignalQuality));
  SetProxy();
  SetService();
  EXPECT_EQ(0, cellular_->service()->strength());
  capability_->GetSignalQuality();
  EXPECT_EQ(kStrength, cellular_->service()->strength());
}

TEST_F(CellularCapabilityCdmaTest, GetRegistrationState) {
  EXPECT_FALSE(cellular_->service().get());
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            capability_->registration_state_1x());
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            capability_->registration_state_evdo());
  EXPECT_CALL(*proxy_,
              GetRegistrationState(nullptr, _,
                                   CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(
          this,
          &CellularCapabilityCdmaTest::InvokeGetRegistrationState));
  SetProxy();
  cellular_->state_ = Cellular::kStateEnabled;
  EXPECT_CALL(*modem_info_.mock_manager(), RegisterService(_));
  capability_->GetRegistrationState();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED,
            capability_->registration_state_1x());
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_HOME,
            capability_->registration_state_evdo());
  EXPECT_TRUE(cellular_->service().get());
}

}  // namespace shill
