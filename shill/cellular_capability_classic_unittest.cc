// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_modem_cdma_proxy.h"
#include "shill/mock_modem_gobi_proxy.h"
#include "shill/mock_modem_gsm_card_proxy.h"
#include "shill/mock_modem_gsm_network_proxy.h"
#include "shill/mock_modem_info.h"
#include "shill/mock_modem_proxy.h"
#include "shill/mock_modem_simple_proxy.h"
#include "shill/mock_profile.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/proxy_factory.h"
#include "shill/testing.h"

using base::Bind;
using base::Unretained;
using std::string;
using testing::InSequence;
using testing::NiceMock;
using testing::_;

namespace shill {

class CellularCapabilityTest : public testing::Test {
 public:
  CellularCapabilityTest()
      : modem_info_(NULL, &dispatcher_, NULL, NULL, NULL),
        create_gsm_card_proxy_from_factory_(false),
        proxy_(new MockModemProxy()),
        simple_proxy_(new MockModemSimpleProxy()),
        cdma_proxy_(new MockModemCDMAProxy()),
        gsm_card_proxy_(new MockModemGSMCardProxy()),
        gsm_network_proxy_(new MockModemGSMNetworkProxy()),
        gobi_proxy_(new MockModemGobiProxy()),
        proxy_factory_(this),
        capability_(NULL),
        device_adaptor_(NULL),
        cellular_(new Cellular(&modem_info_,
                               "",
                               "",
                               0,
                               Cellular::kTypeGSM,
                               "",
                               "",
                               "",
                               &proxy_factory_)) {
    modem_info_.metrics()->RegisterDevice(cellular_->interface_index(),
                            Technology::kCellular);
  }

  virtual ~CellularCapabilityTest() {
    cellular_->service_ = NULL;
    capability_ = NULL;
    device_adaptor_ = NULL;
  }

  virtual void SetUp() {
    static_cast<Device *>(cellular_)->rtnl_handler_ = &rtnl_handler_;

    capability_ = dynamic_cast<CellularCapabilityClassic *>(
        cellular_->capability_.get());
    device_adaptor_ =
        dynamic_cast<DeviceMockAdaptor*>(cellular_->adaptor());
    ASSERT_TRUE(device_adaptor_ != NULL);
  }

  virtual void TearDown() {
    capability_->proxy_factory_ = NULL;
  }

  void CreateService() {
    // The following constants are never directly accessed by the tests.
    const char kStorageIdentifier[] = "default_test_storage_id";
    const char kFriendlyServiceName[] = "default_test_service_name";
    const char kOperatorCode[] = "10010";
    const char kOperatorName[] = "default_test_operator_name";
    const char kOperatorCountry[] = "us";

    // Simulate all the side-effects of Cellular::CreateService
    auto service = new CellularService(&modem_info_, cellular_);
    service->SetStorageIdentifier(kStorageIdentifier);
    service->SetFriendlyName(kFriendlyServiceName);

    Cellular::Operator oper;
    oper.SetCode(kOperatorCode);
    oper.SetName(kOperatorName);
    oper.SetCountry(kOperatorCountry);

    service->SetServingOperator(oper);

    cellular_->set_home_provider(oper);
    cellular_->service_ = service;
  }

  CellularCapabilityGSM *GetGsmCapability() {
    return dynamic_cast<CellularCapabilityGSM *>(cellular_->capability_.get());
  }

  void ReleaseCapabilityProxies() {
    capability_->ReleaseProxies();
  }

  void InvokeEnable(bool enable, Error *error,
                    const ResultCallback &callback, int timeout) {
    callback.Run(Error());
  }
  void InvokeEnableFail(bool enable, Error *error,
                        const ResultCallback &callback, int timeout) {
    callback.Run(Error(Error::kOperationFailed));
  }
  void InvokeDisconnect(Error *error, const ResultCallback &callback,
                        int timeout) {
    callback.Run(Error());
  }
  void InvokeDisconnectFail(Error *error, const ResultCallback &callback,
                            int timeout) {
    callback.Run(Error(Error::kOperationFailed));
  }
  void InvokeGetModemStatus(Error *error,
                            const DBusPropertyMapCallback &callback,
                            int timeout) {
    DBusPropertiesMap props;
    props["carrier"].writer().append_string(kTestCarrier);
    props["unknown-property"].writer().append_string("irrelevant-value");
    callback.Run(props, Error());
  }
  void InvokeGetModemInfo(Error *error, const ModemInfoCallback &callback,
                          int timeout) {
    ModemHardwareInfo info;
    info._1 = kManufacturer;
    info._2 = kModelID;
    info._3 = kHWRev;
    callback.Run(info, Error());
  }
  void InvokeSetCarrier(const string &carrier, Error *error,
                        const ResultCallback &callback, int timeout) {
    callback.Run(Error());
  }

  MOCK_METHOD1(TestCallback, void(const Error &error));

 protected:
  static const char kTestMobileProviderDBPath[];
  static const char kTestCarrier[];
  static const char kManufacturer[];
  static const char kModelID[];
  static const char kHWRev[];

  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(CellularCapabilityTest *test) : test_(test) {}

    virtual ModemProxyInterface *CreateModemProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->proxy_.release();
    }

    virtual ModemSimpleProxyInterface *CreateModemSimpleProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->simple_proxy_.release();
    }

    virtual ModemCDMAProxyInterface *CreateModemCDMAProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->cdma_proxy_.release();
    }

    virtual ModemGSMCardProxyInterface *CreateModemGSMCardProxy(
        const string &/*path*/,
        const string &/*service*/) {
      // TODO(benchan): This code conditionally returns a NULL pointer to avoid
      // CellularCapabilityGSM::InitProperties (and thus
      // CellularCapabilityGSM::GetIMSI) from being called during the
      // construction. Remove this workaround after refactoring the tests.
      return test_->create_gsm_card_proxy_from_factory_ ?
          test_->gsm_card_proxy_.release() : NULL;
    }

    virtual ModemGSMNetworkProxyInterface *CreateModemGSMNetworkProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->gsm_network_proxy_.release();
    }

    virtual ModemGobiProxyInterface *CreateModemGobiProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->gobi_proxy_.release();
    }

   private:
    CellularCapabilityTest *test_;
  };

  void SetProxy() {
    capability_->proxy_.reset(proxy_.release());
  }

  void SetSimpleProxy() {
    capability_->simple_proxy_.reset(simple_proxy_.release());
  }

  void SetGSMNetworkProxy() {
    CellularCapabilityGSM *gsm_capability =
        dynamic_cast<CellularCapabilityGSM *>(cellular_->capability_.get());
    gsm_capability->network_proxy_.reset(gsm_network_proxy_.release());
  }

  void SetCellularType(Cellular::Type type) {
    cellular_->InitCapability(type);
    capability_ = dynamic_cast<CellularCapabilityClassic *>(
        cellular_->capability_.get());
  }

  void AllowCreateGSMCardProxyFromFactory() {
    create_gsm_card_proxy_from_factory_ = true;
  }

  EventDispatcher dispatcher_;
  MockModemInfo modem_info_;
  MockRTNLHandler rtnl_handler_;
  bool create_gsm_card_proxy_from_factory_;
  scoped_ptr<MockModemProxy> proxy_;
  scoped_ptr<MockModemSimpleProxy> simple_proxy_;
  scoped_ptr<MockModemCDMAProxy> cdma_proxy_;
  scoped_ptr<MockModemGSMCardProxy> gsm_card_proxy_;
  scoped_ptr<MockModemGSMNetworkProxy> gsm_network_proxy_;
  scoped_ptr<MockModemGobiProxy> gobi_proxy_;
  TestProxyFactory proxy_factory_;
  CellularCapabilityClassic *capability_;  // Owned by |cellular_|.
  DeviceMockAdaptor *device_adaptor_;  // Owned by |cellular_|.
  CellularRefPtr cellular_;
};

const char CellularCapabilityTest::kTestMobileProviderDBPath[] =
    "provider_db_unittest.bfd";
const char CellularCapabilityTest::kTestCarrier[] = "The Cellular Carrier";
const char CellularCapabilityTest::kManufacturer[] = "Company";
const char CellularCapabilityTest::kModelID[] = "Gobi 2000";
const char CellularCapabilityTest::kHWRev[] = "A00B1234";

TEST_F(CellularCapabilityTest, GetModemStatus) {
  SetCellularType(Cellular::kTypeCDMA);
  EXPECT_CALL(*simple_proxy_,
              GetModemStatus(_, _, CellularCapability::kTimeoutDefault)).
      WillOnce(Invoke(this, &CellularCapabilityTest::InvokeGetModemStatus));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetSimpleProxy();
  ResultCallback callback =
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this));
  capability_->GetModemStatus(callback);
  EXPECT_EQ(kTestCarrier, cellular_->carrier());
}

TEST_F(CellularCapabilityTest, GetModemInfo) {
  EXPECT_CALL(*proxy_, GetModemInfo(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityTest::InvokeGetModemInfo));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetProxy();
  ResultCallback callback =
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this));
  capability_->GetModemInfo(callback);
  EXPECT_EQ(kManufacturer, cellular_->manufacturer());
  EXPECT_EQ(kModelID, cellular_->model_id());
  EXPECT_EQ(kHWRev, cellular_->hardware_revision());
}

TEST_F(CellularCapabilityTest, EnableModemSucceed) {
  EXPECT_CALL(*proxy_, Enable(true, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this, &CellularCapabilityTest::InvokeEnable));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  ResultCallback callback =
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this));
  SetProxy();
  capability_->EnableModem(callback);
}

TEST_F(CellularCapabilityTest, EnableModemFail) {
  EXPECT_CALL(*proxy_, Enable(true, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this, &CellularCapabilityTest::InvokeEnableFail));
  EXPECT_CALL(*this, TestCallback(IsFailure()));
  ResultCallback callback =
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this));
  SetProxy();
  capability_->EnableModem(callback);
}

TEST_F(CellularCapabilityTest, FinishEnable) {
  EXPECT_CALL(*gsm_network_proxy_,
              GetRegistrationInfo(NULL, _,
                                  CellularCapability::kTimeoutDefault));
  EXPECT_CALL(*gsm_network_proxy_,
              GetSignalQuality(NULL, _, CellularCapability::kTimeoutDefault));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetGSMNetworkProxy();
  capability_->FinishEnable(
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this)));
}

TEST_F(CellularCapabilityTest, UnsupportedOperation) {
  Error error;
  EXPECT_CALL(*this, TestCallback(IsSuccess())).Times(0);
  capability_->CellularCapability::Reset(
      &error,
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this)));
  EXPECT_TRUE(error.IsFailure());
  EXPECT_EQ(Error::kNotSupported, error.type());
}

TEST_F(CellularCapabilityTest, AllowRoaming) {
  EXPECT_FALSE(cellular_->GetAllowRoaming(NULL));
  cellular_->SetAllowRoaming(false, NULL);
  EXPECT_FALSE(cellular_->GetAllowRoaming(NULL));

  {
    InSequence seq;
    EXPECT_CALL(*device_adaptor_,
                EmitBoolChanged(kCellularAllowRoamingProperty, true));
    EXPECT_CALL(*device_adaptor_,
                EmitBoolChanged(kCellularAllowRoamingProperty, false));
  }

  cellular_->state_ = Cellular::kStateConnected;
  dynamic_cast<CellularCapabilityGSM *>(capability_)->registration_state_ =
      MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING;
  cellular_->SetAllowRoaming(true, NULL);
  EXPECT_TRUE(cellular_->GetAllowRoaming(NULL));
  EXPECT_EQ(Cellular::kStateConnected, cellular_->state_);

  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDisconnect))
      .WillOnce(Invoke(this, &CellularCapabilityTest::InvokeDisconnect));
  SetProxy();
  cellular_->state_ = Cellular::kStateConnected;
  cellular_->SetAllowRoaming(false, NULL);
  EXPECT_FALSE(cellular_->GetAllowRoaming(NULL));
  EXPECT_EQ(Cellular::kStateRegistered, cellular_->state_);
}

TEST_F(CellularCapabilityTest, SetCarrier) {
  static const char kCarrier[] = "Generic UMTS";
  EXPECT_CALL(
      *gobi_proxy_,
      SetCarrier(kCarrier, _, _,
                 CellularCapabilityClassic::kTimeoutSetCarrierMilliseconds))
      .WillOnce(Invoke(this, &CellularCapabilityTest::InvokeSetCarrier));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  Error error;
  capability_->SetCarrier(kCarrier, &error,
                          Bind(&CellularCapabilityTest::TestCallback,
                               Unretained(this)));
  EXPECT_TRUE(error.IsSuccess());
}

MATCHER_P(HasApn, apn, "") {
  DBusPropertiesMap::const_iterator it = arg.find(kApnProperty);
  return it != arg.end() && apn == it->second.reader().get_string();
}

MATCHER(HasNoApn, "") {
  return arg.find(kApnProperty) == arg.end();
}

TEST_F(CellularCapabilityTest, TryApns) {
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
  DBusPropertiesMap props;
  CellularCapabilityGSM *gsm_capability = GetGsmCapability();

  apn_info[kApnProperty] = kLastGoodApn;
  apn_info[kApnUsernameProperty] = kLastGoodUsername;
  cellular_->service()->SetLastGoodApn(apn_info);
  props.clear();
  EXPECT_TRUE(props.find(kApnProperty) == props.end());
  gsm_capability->SetupConnectProperties(&props);
  // We expect the list to contain the last good APN, plus
  // the 4 APNs from the mobile provider info database.
  EXPECT_EQ(5, gsm_capability->apn_try_list_.size());
  EXPECT_FALSE(props.find(kApnProperty) == props.end());
  EXPECT_EQ(kLastGoodApn, props[kApnProperty].reader().get_string());
  EXPECT_FALSE(props.find(kApnUsernameProperty) == props.end());
  EXPECT_EQ(kLastGoodUsername,
            props[kApnUsernameProperty].reader().get_string());

  apn_info.clear();
  props.clear();
  apn_info[kApnProperty] = kSuppliedApn;
  // Setting the APN has the side effect of clearing the LastGoodApn,
  // so the try list will have 5 elements, with the first one being
  // the supplied APN.
  cellular_->service()->SetApn(apn_info, &error);
  EXPECT_TRUE(props.find(kApnProperty) == props.end());
  gsm_capability->SetupConnectProperties(&props);
  EXPECT_EQ(5, gsm_capability->apn_try_list_.size());
  EXPECT_FALSE(props.find(kApnProperty) == props.end());
  EXPECT_EQ(kSuppliedApn, props[kApnProperty].reader().get_string());

  apn_info.clear();
  props.clear();
  apn_info[kApnProperty] = kLastGoodApn;
  apn_info[kApnUsernameProperty] = kLastGoodUsername;
  // Now when LastGoodAPN is set, it will be the one selected.
  cellular_->service()->SetLastGoodApn(apn_info);
  EXPECT_TRUE(props.find(kApnProperty) == props.end());
  gsm_capability->SetupConnectProperties(&props);
  // We expect the list to contain the last good APN, plus
  // the user-supplied APN, plus the 4 APNs from the mobile
  // provider info database.
  EXPECT_EQ(6, gsm_capability->apn_try_list_.size());
  EXPECT_FALSE(props.find(kApnProperty) == props.end());
  EXPECT_EQ(kLastGoodApn, props[kApnProperty].reader().get_string());

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

TEST_F(CellularCapabilityTest, StopModemDisconnectSuccess) {
  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDisconnect))
      .WillOnce(Invoke(this,
                       &CellularCapabilityTest::InvokeDisconnect));
  EXPECT_CALL(*proxy_, Enable(_, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this,
                       &CellularCapabilityTest::InvokeEnable));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetProxy();

  Error error;
  capability_->StopModem(
      &error, Bind(&CellularCapabilityTest::TestCallback, Unretained(this)));
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityTest, StopModemDisconnectFail) {
  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDisconnect))
      .WillOnce(Invoke(this,
                       &CellularCapabilityTest::InvokeDisconnectFail));
  EXPECT_CALL(*proxy_, Enable(_, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this,
                       &CellularCapabilityTest::InvokeEnable));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetProxy();

  Error error;
  capability_->StopModem(
      &error, Bind(&CellularCapabilityTest::TestCallback, Unretained(this)));
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityTest, DisconnectNoProxy) {
  Error error;
  ResultCallback disconnect_callback;
  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDisconnect))
      .Times(0);
  ReleaseCapabilityProxies();
  capability_->Disconnect(&error, disconnect_callback);
}

}  // namespace shill
