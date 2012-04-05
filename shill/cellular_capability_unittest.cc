// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_modem_cdma_proxy.h"
#include "shill/mock_modem_gsm_card_proxy.h"
#include "shill/mock_modem_gsm_network_proxy.h"
#include "shill/mock_modem_proxy.h"
#include "shill/mock_modem_simple_proxy.h"
#include "shill/mock_profile.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using base::Bind;
using base::Unretained;
using std::string;
using testing::InSequence;
using testing::NiceMock;
using testing::_;

namespace shill {

MATCHER(IsSuccess, "") {
  return arg.IsSuccess();
}
MATCHER(IsFailure, "") {
  return arg.IsFailure();
}

class CellularCapabilityTest : public testing::Test {
 public:
  CellularCapabilityTest()
      : manager_(&control_, &dispatcher_, &metrics_, &glib_),
        cellular_(new Cellular(&control_,
                               &dispatcher_,
                               NULL,
                               &manager_,
                               "",
                               "",
                               0,
                               Cellular::kTypeGSM,
                               "",
                               "",
                               NULL)),
        proxy_(new MockModemProxy()),
        simple_proxy_(new MockModemSimpleProxy()),
        cdma_proxy_(new MockModemCDMAProxy()),
        gsm_card_proxy_(new MockModemGSMCardProxy()),
        gsm_network_proxy_(new MockModemGSMNetworkProxy()),
        proxy_factory_(this),
        capability_(NULL),
        device_adaptor_(NULL),
        provider_db_(NULL) {}

  virtual ~CellularCapabilityTest() {
    cellular_->service_ = NULL;
    capability_ = NULL;
    device_adaptor_ = NULL;
    mobile_provider_close_db(provider_db_);
    provider_db_ = NULL;
  }

  virtual void SetUp() {
    static_cast<Device *>(cellular_)->rtnl_handler_ = &rtnl_handler_;
    capability_ = cellular_->capability_.get();
    device_adaptor_ =
        dynamic_cast<NiceMock<DeviceMockAdaptor> *>(cellular_->adaptor());
  }

  virtual void TearDown() {
    capability_->proxy_factory_ = NULL;
  }

  void InitProviderDB() {
    provider_db_ = mobile_provider_open_db(kTestMobileProviderDBPath);
    ASSERT_TRUE(provider_db_);
    cellular_->provider_db_ = provider_db_;
  }

  void SetService() {
    cellular_->service_ = new CellularService(
        &control_, &dispatcher_, &metrics_, NULL, cellular_);
  }

  CellularCapabilityGSM *GetGsmCapability() {
    return dynamic_cast<CellularCapabilityGSM *>(cellular_->capability_.get());
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
      return test_->gsm_card_proxy_.release();
    }

    virtual ModemGSMNetworkProxyInterface *CreateModemGSMNetworkProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->gsm_network_proxy_.release();
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
    cellular_->InitCapability(type, &proxy_factory_);
    capability_ = cellular_->capability_.get();
  }

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;
  CellularRefPtr cellular_;
  MockRTNLHandler rtnl_handler_;
  scoped_ptr<MockModemProxy> proxy_;
  scoped_ptr<MockModemSimpleProxy> simple_proxy_;
  scoped_ptr<MockModemCDMAProxy> cdma_proxy_;
  scoped_ptr<MockModemGSMCardProxy> gsm_card_proxy_;
  scoped_ptr<MockModemGSMNetworkProxy> gsm_network_proxy_;
  TestProxyFactory proxy_factory_;
  CellularCapability *capability_;  // Owned by |cellular_|.
  NiceMock<DeviceMockAdaptor> *device_adaptor_;  // Owned by |cellular_|.
  mobile_provider_db *provider_db_;
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
  EXPECT_EQ(kTestCarrier, capability_->carrier_);
  EXPECT_EQ(kTestCarrier, cellular_->home_provider_.GetName());
}

TEST_F(CellularCapabilityTest, GetModemInfo) {
  EXPECT_CALL(*proxy_, GetModemInfo(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityTest::InvokeGetModemInfo));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetProxy();
  ResultCallback callback =
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this));
  capability_->GetModemInfo(callback);
  EXPECT_EQ(kManufacturer, capability_->manufacturer_);
  EXPECT_EQ(kModelID, capability_->model_id_);
  EXPECT_EQ(kHWRev, capability_->hardware_revision_);
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
  capability_->CellularCapability::Scan(&error,
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this)));
  EXPECT_TRUE(error.IsFailure());
  EXPECT_EQ(Error::kNotSupported, error.type());
}

TEST_F(CellularCapabilityTest, AllowRoaming) {
  EXPECT_FALSE(capability_->GetAllowRoaming(NULL));
  capability_->SetAllowRoaming(false, NULL);
  EXPECT_FALSE(capability_->GetAllowRoaming(NULL));

  {
    InSequence seq;
    EXPECT_CALL(*device_adaptor_, EmitBoolChanged(
        flimflam::kCellularAllowRoamingProperty, true));
    EXPECT_CALL(*device_adaptor_, EmitBoolChanged(
        flimflam::kCellularAllowRoamingProperty, false));
  }

  cellular_->state_ = Cellular::kStateConnected;
  dynamic_cast<CellularCapabilityGSM *>(capability_)->registration_state_ =
      MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING;
  capability_->SetAllowRoaming(true, NULL);
  EXPECT_TRUE(capability_->GetAllowRoaming(NULL));
  EXPECT_EQ(Cellular::kStateConnected, cellular_->state_);

  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityTest::InvokeDisconnect));
  SetProxy();
  cellular_->state_ = Cellular::kStateConnected;
  capability_->SetAllowRoaming(false, NULL);
  EXPECT_FALSE(capability_->GetAllowRoaming(NULL));
  EXPECT_EQ(Cellular::kStateRegistered, cellular_->state_);
}

MATCHER_P(HasApn, apn, "") {
  DBusPropertiesMap::const_iterator it = arg.find(flimflam::kApnProperty);
  return it != arg.end() && apn == it->second.reader().get_string();
}

MATCHER(HasNoApn, "") {
  return arg.find(flimflam::kApnProperty) == arg.end();
}

TEST_F(CellularCapabilityTest, TryApns) {
  static const string kLastGoodApn("remembered.apn");
  static const string kSuppliedApn("my.apn");
  static const string kTmobileApn1("epc.tmobile.com");
  static const string kTmobileApn2("wap.voicestream.com");
  static const string kTmobileApn3("internet2.voicestream.com");
  static const string kTmobileApn4("internet3.voicestream.com");

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
  CellularCapabilityGSM *gsm_capability = GetGsmCapability();
  SetService();
  gsm_capability->imsi_ = "310240123456789";
  InitProviderDB();
  gsm_capability->SetHomeProvider();
  ProfileRefPtr profile(new NiceMock<MockProfile>(
      &control_, reinterpret_cast<Manager *>(NULL)));
  cellular_->service()->set_profile(profile);

  Error error;
  Stringmap apn_info;
  DBusPropertiesMap props;
  apn_info[flimflam::kApnProperty] = kSuppliedApn;
  cellular_->service()->SetApn(apn_info, &error);

  apn_info.clear();
  apn_info[flimflam::kApnProperty] = kLastGoodApn;
  cellular_->service()->SetLastGoodApn(apn_info);

  capability_->SetupConnectProperties(&props);
  // We expect the list to contain the last good APN, plus
  // the user-supplied APN, plus the 4 APNs from the mobile
  // provider info database.
  EXPECT_EQ(6, gsm_capability->apn_try_list_.size());
  EXPECT_FALSE(props.find(flimflam::kApnProperty) == props.end());
  EXPECT_EQ(kLastGoodApn, props[flimflam::kApnProperty].reader().get_string());

  SetSimpleProxy();
  capability_->Connect(props, &error, ResultCallback());
  Error cerror(Error::kInvalidApn);
  capability_->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(5, gsm_capability->apn_try_list_.size());
  capability_->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(4, gsm_capability->apn_try_list_.size());
  capability_->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(3, gsm_capability->apn_try_list_.size());
  capability_->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(2, gsm_capability->apn_try_list_.size());
  capability_->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(1, gsm_capability->apn_try_list_.size());
  capability_->OnConnectReply(ResultCallback(), cerror);
  EXPECT_EQ(0, gsm_capability->apn_try_list_.size());
}

TEST_F(CellularCapabilityTest, StopModemDisconnectSuccess) {
  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this,
                       &CellularCapabilityTest::InvokeDisconnect));
  EXPECT_CALL(*proxy_, Enable(_, _, _, CellularCapability::kTimeoutDefault))
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
  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this,
                       &CellularCapabilityTest::InvokeDisconnectFail));
  EXPECT_CALL(*proxy_, Enable(_, _, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this,
                       &CellularCapabilityTest::InvokeEnable));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetProxy();

  Error error;
  capability_->StopModem(
      &error, Bind(&CellularCapabilityTest::TestCallback, Unretained(this)));
  dispatcher_.DispatchPendingEvents();
}

}  // namespace shill
