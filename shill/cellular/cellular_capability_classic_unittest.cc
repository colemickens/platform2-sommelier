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

#include "shill/cellular/cellular_capability_gsm.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>

#include "shill/cellular/cellular.h"
#include "shill/cellular/cellular_service.h"
#include "shill/cellular/mock_modem_cdma_proxy.h"
#include "shill/cellular/mock_modem_gobi_proxy.h"
#include "shill/cellular/mock_modem_gsm_card_proxy.h"
#include "shill/cellular/mock_modem_gsm_network_proxy.h"
#include "shill/cellular/mock_modem_info.h"
#include "shill/cellular/mock_modem_proxy.h"
#include "shill/cellular/mock_modem_simple_proxy.h"
#include "shill/error.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"
#include "shill/mock_profile.h"
#include "shill/net/mock_rtnl_handler.h"
#include "shill/test_event_dispatcher.h"
#include "shill/testing.h"

using base::Bind;
using base::Unretained;
using std::string;
using testing::InSequence;
using testing::NiceMock;
using testing::_;

namespace shill {

class CellularCapabilityClassicTest
    : public testing::TestWithParam<Cellular::Type> {
 public:
  CellularCapabilityClassicTest()
      : control_interface_(this),
        modem_info_(&control_interface_, &dispatcher_, nullptr, nullptr),
        create_gsm_card_proxy_from_factory_(false),
        proxy_(new MockModemProxy()),
        simple_proxy_(new MockModemSimpleProxy()),
        cdma_proxy_(new MockModemCdmaProxy()),
        gsm_card_proxy_(new MockModemGsmCardProxy()),
        gsm_network_proxy_(new MockModemGsmNetworkProxy()),
        gobi_proxy_(new MockModemGobiProxy()),
        capability_(nullptr),
        device_adaptor_(nullptr),
        cellular_(new Cellular(&modem_info_,
                               "",
                               "",
                               0,
                               GetParam(),
                               "",
                               "")) {
    modem_info_.metrics()->RegisterDevice(cellular_->interface_index(),
                            Technology::kCellular);
  }

  ~CellularCapabilityClassicTest() override {
    cellular_->service_ = nullptr;
    capability_ = nullptr;
    device_adaptor_ = nullptr;
  }

  void SetUp() override {
    static_cast<Device*>(cellular_.get())->rtnl_handler_ = &rtnl_handler_;

    capability_ = static_cast<CellularCapabilityClassic*>(
        cellular_->capability_.get());
    device_adaptor_ =
        static_cast<DeviceMockAdaptor*>(cellular_->adaptor());
    ASSERT_NE(nullptr, device_adaptor_);;
  }

  // TODO(benchan): Instead of conditionally enabling many tests for specific
  // capability types via IsCellularTypeUnderTestOneOf, migrate more tests to
  // work under all capability types and proboably migrate those tests for
  // specific capability types into their own test fixture subclasses.
  bool IsCellularTypeUnderTestOneOf(
      const std::set<Cellular::Type>& valid_types) const {
    return ContainsValue(valid_types, GetParam());
  }

  void CreateService() {
    // The following constants are never directly accessed by the tests.
    const char kFriendlyServiceName[] = "default_test_service_name";
    const char kOperatorCode[] = "10010";
    const char kOperatorName[] = "default_test_operator_name";
    const char kOperatorCountry[] = "us";

    // Simulate all the side-effects of Cellular::CreateService
    auto service = new CellularService(&modem_info_, cellular_);
    service->SetFriendlyName(kFriendlyServiceName);

    Stringmap serving_operator;
    serving_operator[kOperatorCodeKey] = kOperatorCode;
    serving_operator[kOperatorNameKey] = kOperatorName;
    serving_operator[kOperatorCountryKey] = kOperatorCountry;

    service->set_serving_operator(serving_operator);
    cellular_->set_home_provider(serving_operator);
    cellular_->service_ = service;
  }

  CellularCapabilityGsm* GetGsmCapability() {
    return static_cast<CellularCapabilityGsm*>(cellular_->capability_.get());
  }

  void ReleaseCapabilityProxies() {
    capability_->ReleaseProxies();
  }

  void InvokeEnable(bool enable, Error* error,
                    const ResultCallback& callback, int timeout) {
    callback.Run(Error());
  }
  void InvokeEnableFail(bool enable, Error* error,
                        const ResultCallback& callback, int timeout) {
    callback.Run(Error(Error::kOperationFailed));
  }
  void InvokeDisconnect(Error* error, const ResultCallback& callback,
                        int timeout) {
    callback.Run(Error());
  }
  void InvokeDisconnectFail(Error* error, const ResultCallback& callback,
                            int timeout) {
    callback.Run(Error(Error::kOperationFailed));
  }
  void InvokeGetModemStatus(Error* error,
                            const KeyValueStoreCallback& callback,
                            int timeout) {
    KeyValueStore props;
    props.SetString("carrier", kTestCarrier);
    props.SetString("unknown-property", "irrelevant-value");
    callback.Run(props, Error());
  }
  void InvokeGetModemInfo(Error* error, const ModemInfoCallback& callback,
                          int timeout) {
    callback.Run(kManufacturer, kModelID, kHWRev, Error());
  }
  void InvokeSetCarrier(const string& carrier, Error* error,
                        const ResultCallback& callback, int timeout) {
    callback.Run(Error());
  }

  MOCK_METHOD1(TestCallback, void(const Error& error));

 protected:
  static const char kTestMobileProviderDBPath[];
  static const char kTestCarrier[];
  static const char kManufacturer[];
  static const char kModelID[];
  static const char kHWRev[];

  class TestControl : public MockControl {
   public:
    explicit TestControl(CellularCapabilityClassicTest* test) : test_(test) {}

    std::unique_ptr<ModemProxyInterface> CreateModemProxy(
        const string& /*path*/,
        const string& /*service*/) override {
      return std::move(test_->proxy_);
    }

    std::unique_ptr<ModemSimpleProxyInterface> CreateModemSimpleProxy(
        const string& /*path*/,
        const string& /*service*/) override {
      return std::move(test_->simple_proxy_);
    }

    std::unique_ptr<ModemCdmaProxyInterface> CreateModemCdmaProxy(
        const string& /*path*/,
        const string& /*service*/) override {
      return std::move(test_->cdma_proxy_);
    }

    std::unique_ptr<ModemGsmCardProxyInterface> CreateModemGsmCardProxy(
        const string& /*path*/,
        const string& /*service*/) override {
      // TODO(benchan): This code conditionally returns a nullptr to avoid
      // CellularCapabilityGsm::InitProperties (and thus
      // CellularCapabilityGsm::GetIMSI) from being called during the
      // construction. Remove this workaround after refactoring the tests.
      if (test_->create_gsm_card_proxy_from_factory_) {
        return std::move(test_->gsm_card_proxy_);
      }
      return nullptr;
    }

    std::unique_ptr<ModemGsmNetworkProxyInterface> CreateModemGsmNetworkProxy(
        const string& /*path*/, const string& /*service*/) override {
      return std::move(test_->gsm_network_proxy_);
    }

    std::unique_ptr<ModemGobiProxyInterface> CreateModemGobiProxy(
        const string& /*path*/,
        const string& /*service*/) override {
      return std::move(test_->gobi_proxy_);
    }

   private:
    CellularCapabilityClassicTest* test_;
  };

  void SetProxy() {
    capability_->proxy_ = std::move(proxy_);
  }

  void SetSimpleProxy() {
    capability_->simple_proxy_ = std::move(simple_proxy_);
  }

  void SetGsmNetworkProxy() {
    CellularCapabilityGsm* gsm_capability =
        static_cast<CellularCapabilityGsm*>(cellular_->capability_.get());
    gsm_capability->network_proxy_ = std::move(gsm_network_proxy_);
  }

  void AllowCreateGsmCardProxyFromFactory() {
    create_gsm_card_proxy_from_factory_ = true;
  }

  EventDispatcherForTest dispatcher_;
  TestControl control_interface_;
  MockModemInfo modem_info_;
  MockRTNLHandler rtnl_handler_;
  bool create_gsm_card_proxy_from_factory_;
  std::unique_ptr<MockModemProxy> proxy_;
  std::unique_ptr<MockModemSimpleProxy> simple_proxy_;
  std::unique_ptr<MockModemCdmaProxy> cdma_proxy_;
  std::unique_ptr<MockModemGsmCardProxy> gsm_card_proxy_;
  std::unique_ptr<MockModemGsmNetworkProxy> gsm_network_proxy_;
  std::unique_ptr<MockModemGobiProxy> gobi_proxy_;
  CellularCapabilityClassic* capability_;  // Owned by |cellular_|.
  DeviceMockAdaptor* device_adaptor_;  // Owned by |cellular_|.
  CellularRefPtr cellular_;
};

const char CellularCapabilityClassicTest::kTestMobileProviderDBPath[] =
    "provider_db_unittest.bfd";
const char CellularCapabilityClassicTest::kTestCarrier[] =
    "The Cellular Carrier";
const char CellularCapabilityClassicTest::kManufacturer[] = "Company";
const char CellularCapabilityClassicTest::kModelID[] = "Gobi 2000";
const char CellularCapabilityClassicTest::kHWRev[] = "A00B1234";

TEST_P(CellularCapabilityClassicTest, GetModemStatus) {
  EXPECT_CALL(*simple_proxy_,
              GetModemStatus(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(
          Invoke(this, &CellularCapabilityClassicTest::InvokeGetModemStatus));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetSimpleProxy();
  ResultCallback callback =
      Bind(&CellularCapabilityClassicTest::TestCallback, Unretained(this));
  capability_->GetModemStatus(callback);
  EXPECT_EQ(kTestCarrier, cellular_->carrier());
}

TEST_P(CellularCapabilityClassicTest, GetModemInfo) {
  if (!IsCellularTypeUnderTestOneOf({Cellular::kTypeGsm})) {
    return;
  }

  EXPECT_CALL(*proxy_, GetModemInfo(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(
          Invoke(this, &CellularCapabilityClassicTest::InvokeGetModemInfo));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetProxy();
  ResultCallback callback =
      Bind(&CellularCapabilityClassicTest::TestCallback, Unretained(this));
  capability_->GetModemInfo(callback);
  EXPECT_EQ(kManufacturer, cellular_->manufacturer());
  EXPECT_EQ(kModelID, cellular_->model_id());
  EXPECT_EQ(kHWRev, cellular_->hardware_revision());
}

TEST_P(CellularCapabilityClassicTest, EnableModemSucceed) {
  EXPECT_CALL(*proxy_, Enable(true, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this, &CellularCapabilityClassicTest::InvokeEnable));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  ResultCallback callback =
      Bind(&CellularCapabilityClassicTest::TestCallback, Unretained(this));
  SetProxy();
  capability_->EnableModem(callback);
}

TEST_P(CellularCapabilityClassicTest, EnableModemFail) {
  EXPECT_CALL(*proxy_, Enable(true, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this, &CellularCapabilityClassicTest::InvokeEnableFail));
  EXPECT_CALL(*this, TestCallback(IsFailure()));
  ResultCallback callback =
      Bind(&CellularCapabilityClassicTest::TestCallback, Unretained(this));
  SetProxy();
  capability_->EnableModem(callback);
}

TEST_P(CellularCapabilityClassicTest, FinishEnable) {
  if (!IsCellularTypeUnderTestOneOf({Cellular::kTypeGsm})) {
    return;
  }

  EXPECT_CALL(*gsm_network_proxy_,
              GetRegistrationInfo(nullptr, _,
                                  CellularCapability::kTimeoutDefault));
  EXPECT_CALL(
      *gsm_network_proxy_,
      GetSignalQuality(nullptr, _, CellularCapability::kTimeoutDefault));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetGsmNetworkProxy();
  capability_->FinishEnable(
      Bind(&CellularCapabilityClassicTest::TestCallback, Unretained(this)));
}

TEST_P(CellularCapabilityClassicTest, UnsupportedOperation) {
  Error error;
  EXPECT_CALL(*this, TestCallback(IsSuccess())).Times(0);
  capability_->CellularCapability::Reset(
      &error,
      Bind(&CellularCapabilityClassicTest::TestCallback, Unretained(this)));
  EXPECT_TRUE(error.IsFailure());
  EXPECT_EQ(Error::kNotSupported, error.type());
}

TEST_P(CellularCapabilityClassicTest, AllowRoaming) {
  if (!IsCellularTypeUnderTestOneOf({Cellular::kTypeGsm})) {
    return;
  }

  EXPECT_FALSE(cellular_->GetAllowRoaming(nullptr));
  cellular_->SetAllowRoaming(false, nullptr);
  EXPECT_FALSE(cellular_->GetAllowRoaming(nullptr));

  {
    InSequence seq;
    EXPECT_CALL(*device_adaptor_,
                EmitBoolChanged(kCellularAllowRoamingProperty, true));
    EXPECT_CALL(*device_adaptor_,
                EmitBoolChanged(kCellularAllowRoamingProperty, false));
  }

  cellular_->state_ = Cellular::kStateConnected;
  static_cast<CellularCapabilityGsm*>(capability_)->registration_state_ =
      MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING;
  cellular_->SetAllowRoaming(true, nullptr);
  EXPECT_TRUE(cellular_->GetAllowRoaming(nullptr));
  EXPECT_EQ(Cellular::kStateConnected, cellular_->state_);

  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDisconnect))
      .WillOnce(Invoke(this, &CellularCapabilityClassicTest::InvokeDisconnect));
  SetProxy();
  cellular_->state_ = Cellular::kStateConnected;
  cellular_->SetAllowRoaming(false, nullptr);
  EXPECT_FALSE(cellular_->GetAllowRoaming(nullptr));
  EXPECT_EQ(Cellular::kStateRegistered, cellular_->state_);
}

TEST_P(CellularCapabilityClassicTest, SetCarrier) {
  static const char kCarrier[] = "Generic UMTS";
  EXPECT_CALL(
      *gobi_proxy_,
      SetCarrier(kCarrier, _, _,
                 CellularCapabilityClassic::kTimeoutSetCarrierMilliseconds))
      .WillOnce(Invoke(this, &CellularCapabilityClassicTest::InvokeSetCarrier));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  Error error;
  capability_->SetCarrier(kCarrier, &error,
                          Bind(&CellularCapabilityClassicTest::TestCallback,
                               Unretained(this)));
  EXPECT_TRUE(error.IsSuccess());
}

MATCHER_P(HasApn, apn, "") {
  return arg.ContainsString(kApnProperty) && apn == arg.GetString(kApnProperty);
}

MATCHER(HasNoApn, "") {
  return !arg.ContainsString(kApnProperty);
}

TEST_P(CellularCapabilityClassicTest, TryApns) {
  if (!IsCellularTypeUnderTestOneOf({Cellular::kTypeGsm})) {
    return;
  }

  static const string kLastGoodApn("remembered.apn");
  static const string kLastGoodUsername("remembered.user");
  static const string kSuppliedApn("my.apn");
  static const string kTmobileApn1("epc.tmobile.com");
  static const string kTmobileApn2("wap.voicestream.com");
  static const string kTmobileApn3("internet2.voicestream.com");
  static const string kTmobileApn4("internet3.voicestream.com");
  const Stringmaps kDatabaseApnList {{{ kApnProperty, kTmobileApn1 }},
                                     {{ kApnProperty, kTmobileApn2 }},
                                     {{ kApnProperty, kTmobileApn3 }},
                                     {{ kApnProperty, kTmobileApn4 }}};


  CreateService();
  // Supply the database APNs to |cellular_| object.
  cellular_->set_apn_list(kDatabaseApnList);
  ProfileRefPtr profile(new NiceMock<MockProfile>(
      modem_info_.control_interface(), modem_info_.metrics(),
      modem_info_.manager()));
  cellular_->service()->set_profile(profile);

  Error error;
  Stringmap apn_info;
  KeyValueStore props;
  CellularCapabilityGsm* gsm_capability = GetGsmCapability();

  apn_info[kApnProperty] = kLastGoodApn;
  apn_info[kApnUsernameProperty] = kLastGoodUsername;
  cellular_->service()->SetLastGoodApn(apn_info);
  props.Clear();
  EXPECT_TRUE(props.IsEmpty());
  gsm_capability->SetupConnectProperties(&props);
  // We expect the list to contain the last good APN, plus
  // the 4 APNs from the mobile provider info database.
  EXPECT_EQ(5, gsm_capability->apn_try_list_.size());
  EXPECT_TRUE(props.ContainsString(kApnProperty));
  EXPECT_EQ(kLastGoodApn, props.GetString(kApnProperty));
  EXPECT_TRUE(props.ContainsString(kApnUsernameProperty));
  EXPECT_EQ(kLastGoodUsername,
            props.GetString(kApnUsernameProperty));

  apn_info.clear();
  props.Clear();
  apn_info[kApnProperty] = kSuppliedApn;
  // Setting the APN has the side effect of clearing the LastGoodApn,
  // so the try list will have 5 elements, with the first one being
  // the supplied APN.
  cellular_->service()->SetApn(apn_info, &error);
  EXPECT_TRUE(props.IsEmpty());
  gsm_capability->SetupConnectProperties(&props);
  EXPECT_EQ(5, gsm_capability->apn_try_list_.size());
  EXPECT_TRUE(props.ContainsString(kApnProperty));
  EXPECT_EQ(kSuppliedApn, props.GetString(kApnProperty));

  apn_info.clear();
  props.Clear();
  apn_info[kApnProperty] = kLastGoodApn;
  apn_info[kApnUsernameProperty] = kLastGoodUsername;
  // Now when LastGoodAPN is set, it will be the one selected.
  cellular_->service()->SetLastGoodApn(apn_info);
  EXPECT_TRUE(props.IsEmpty());
  gsm_capability->SetupConnectProperties(&props);
  // We expect the list to contain the last good APN, plus
  // the user-supplied APN, plus the 4 APNs from the mobile
  // provider info database.
  EXPECT_EQ(6, gsm_capability->apn_try_list_.size());
  EXPECT_TRUE(props.ContainsString(kApnProperty));
  EXPECT_EQ(kLastGoodApn, props.GetString(kApnProperty));

  // Now try all the given APNs.
  using testing::InSequence;
  {
    InSequence dummy;
    EXPECT_CALL(*simple_proxy_, Connect(HasApn(kLastGoodApn), _, _, _));
    EXPECT_CALL(*simple_proxy_, Connect(HasApn(kSuppliedApn), _, _, _));
    EXPECT_CALL(*simple_proxy_, Connect(HasApn(kTmobileApn1), _, _, _));
    EXPECT_CALL(*simple_proxy_, Connect(HasApn(kTmobileApn2), _, _, _));
    EXPECT_CALL(*simple_proxy_, Connect(HasApn(kTmobileApn3), _, _, _));
    EXPECT_CALL(*simple_proxy_, Connect(HasApn(kTmobileApn4), _, _, _));
    EXPECT_CALL(*simple_proxy_, Connect(HasNoApn(), _, _, _));
  }
  SetSimpleProxy();
  gsm_capability->Connect(props, &error, ResultCallback());
  Error cerror(Error::kInvalidApn);
  gsm_capability->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(5, gsm_capability->apn_try_list_.size());
  gsm_capability->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(4, gsm_capability->apn_try_list_.size());
  gsm_capability->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(3, gsm_capability->apn_try_list_.size());
  gsm_capability->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(2, gsm_capability->apn_try_list_.size());
  gsm_capability->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(1, gsm_capability->apn_try_list_.size());
  gsm_capability->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(0, gsm_capability->apn_try_list_.size());
}

TEST_P(CellularCapabilityClassicTest, StopModemDisconnectSuccess) {
  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDisconnect))
      .WillOnce(Invoke(this,
                       &CellularCapabilityClassicTest::InvokeDisconnect));
  EXPECT_CALL(*proxy_, Enable(_, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this,
                       &CellularCapabilityClassicTest::InvokeEnable));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetProxy();

  Error error;
  capability_->StopModem(
      &error,
      Bind(&CellularCapabilityClassicTest::TestCallback, Unretained(this)));
  dispatcher_.DispatchPendingEvents();
}

TEST_P(CellularCapabilityClassicTest, StopModemDisconnectFail) {
  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDisconnect))
      .WillOnce(Invoke(this,
                       &CellularCapabilityClassicTest::InvokeDisconnectFail));
  EXPECT_CALL(*proxy_, Enable(_, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this,
                       &CellularCapabilityClassicTest::InvokeEnable));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetProxy();

  Error error;
  capability_->StopModem(
      &error,
      Bind(&CellularCapabilityClassicTest::TestCallback, Unretained(this)));
  dispatcher_.DispatchPendingEvents();
}

TEST_P(CellularCapabilityClassicTest, DisconnectNoProxy) {
  Error error;
  ResultCallback disconnect_callback;
  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDisconnect))
      .Times(0);
  ReleaseCapabilityProxies();
  capability_->Disconnect(&error, disconnect_callback);
}

INSTANTIATE_TEST_CASE_P(CellularCapabilityClassicTest,
                        CellularCapabilityClassicTest,
                        testing::Values(Cellular::kTypeGsm,
                                        Cellular::kTypeCdma));

}  // namespace shill
