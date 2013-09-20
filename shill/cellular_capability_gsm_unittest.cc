// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_log.h"
#include "shill/mock_modem_gsm_card_proxy.h"
#include "shill/mock_modem_gsm_network_proxy.h"
#include "shill/mock_modem_info.h"
#include "shill/mock_modem_proxy.h"
#include "shill/mock_modem_simple_proxy.h"
#include "shill/mock_profile.h"
#include "shill/proxy_factory.h"

using base::Bind;
using base::StringPrintf;
using base::Unretained;
using std::string;
using std::vector;
using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrEq;

namespace shill {

MATCHER(IsSuccess, "") {
  return arg.IsSuccess();
}

MATCHER(IsFailure, "") {
  return arg.IsFailure();
}

class CellularCapabilityGSMTest : public testing::Test {
 public:
  CellularCapabilityGSMTest()
      : modem_info_(NULL, &dispatcher_, NULL, NULL, NULL),
        create_card_proxy_from_factory_(false),
        proxy_(new MockModemProxy()),
        simple_proxy_(new MockModemSimpleProxy()),
        card_proxy_(new MockModemGSMCardProxy()),
        network_proxy_(new MockModemGSMNetworkProxy()),
        proxy_factory_(this),
        capability_(NULL),
        device_adaptor_(NULL),
        cellular_(new Cellular(&modem_info_,
                               "",
                               kAddress,
                               0,
                               Cellular::kTypeGSM,
                               "",
                               "",
                               "",
                               &proxy_factory_)) {
    modem_info_.metrics()->RegisterDevice(cellular_->interface_index(),
                                          Technology::kCellular);
  }

  virtual ~CellularCapabilityGSMTest() {
    cellular_->service_ = NULL;
    capability_ = NULL;
    device_adaptor_ = NULL;
  }

  virtual void SetUp() {
    capability_ =
        dynamic_cast<CellularCapabilityGSM *>(cellular_->capability_.get());
    device_adaptor_ =
        dynamic_cast<DeviceMockAdaptor *>(cellular_->adaptor());
  }

  void InitProviderDB() {
    modem_info_.SetProviderDB(kTestMobileProviderDBPath);
  }

  void InvokeEnable(bool enable, Error *error,
                    const ResultCallback &callback, int timeout) {
    callback.Run(Error());
  }
  void InvokeGetIMEI(Error *error, const GSMIdentifierCallback &callback,
                     int timeout) {
    callback.Run(kIMEI, Error());
  }
  void InvokeGetIMSI(Error *error, const GSMIdentifierCallback &callback,
                     int timeout) {
    callback.Run(kIMSI, Error());
  }
  void InvokeGetIMSI2(Error *error, const GSMIdentifierCallback &callback,
                      int timeout) {
    callback.Run("310240123456789", Error());
  }
  void InvokeGetIMSIFails(Error *error, const GSMIdentifierCallback &callback,
                          int timeout) {
    callback.Run("", Error(Error::kOperationFailed));
  }
  void InvokeGetMSISDN(Error *error, const GSMIdentifierCallback &callback,
                       int timeout) {
    callback.Run(kMSISDN, Error());
  }
  void InvokeGetMSISDNFail(Error *error, const GSMIdentifierCallback &callback,
                           int timeout) {
    callback.Run("", Error(Error::kOperationFailed));
  }
  void InvokeGetSPN(Error *error, const GSMIdentifierCallback &callback,
                    int timeout) {
    callback.Run(kTestCarrier, Error());
  }
  void InvokeGetSPNFail(Error *error, const GSMIdentifierCallback &callback,
                        int timeout) {
    callback.Run("", Error(Error::kOperationFailed));
  }
  void InvokeGetSignalQuality(Error *error,
                              const SignalQualityCallback &callback,
                              int timeout) {
    callback.Run(kStrength, Error());
  }
  void InvokeGetRegistrationInfo(Error *error,
                                 const RegistrationInfoCallback &callback,
                                 int timeout) {
    callback.Run(MM_MODEM_GSM_NETWORK_REG_STATUS_HOME,
                 kTestNetwork, kTestCarrier, Error());
  }
  void InvokeRegister(const string &network_id,
                      Error *error,
                      const ResultCallback &callback,
                      int timeout) {
    callback.Run(Error());
  }
  void InvokeEnablePIN(const string &pin, bool enable,
                       Error *error, const ResultCallback &callback,
                       int timeout) {
    callback.Run(Error());
  }
  void InvokeSendPIN(const string &pin, Error *error,
                     const ResultCallback &callback, int timeout) {
    callback.Run(Error());
  }
  void InvokeSendPUK(const string &puk, const string &pin, Error *error,
                     const ResultCallback &callback, int timeout) {
    callback.Run(Error());
  }
  void InvokeChangePIN(const string &old_pin, const string &pin, Error *error,
                       const ResultCallback &callback, int timeout) {
    callback.Run(Error());
  }
  void InvokeScanReply() {
    GSMScanResults results;
    results.push_back(GSMScanResult());
    results[0][CellularCapabilityGSM::kNetworkPropertyID] = kScanID0;
    results.push_back(GSMScanResult());
    results[1][CellularCapabilityGSM::kNetworkPropertyID] = kScanID1;
    scan_callback_.Run(results, Error());
  }
  void InvokeGetModemStatus(Error *error,
                            const DBusPropertyMapCallback &callback,
                            int timeout) {
    DBusPropertiesMap props;
    callback.Run(props, Error());
  }
  void InvokeGetModemInfo(Error *error, const ModemInfoCallback &callback,
                          int timeout) {
    ModemHardwareInfo info;
    callback.Run(info, Error());
  }

  void InvokeConnectFail(DBusPropertiesMap props, Error *error,
                         const ResultCallback &callback, int timeout) {
    callback.Run(Error(Error::kOperationFailed));
  }

  MOCK_METHOD1(TestCallback, void(const Error &error));

 protected:
  static const char kAddress[];
  static const char kTestMobileProviderDBPath[];
  static const char kTestNetwork[];
  static const char kTestCarrier[];
  static const char kPIN[];
  static const char kPUK[];
  static const char kIMEI[];
  static const char kIMSI[];
  static const char kMSISDN[];
  static const char kScanID0[];
  static const char kScanID1[];
  static const int kStrength;

  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(CellularCapabilityGSMTest *test) : test_(test) {}

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

    virtual ModemGSMCardProxyInterface *CreateModemGSMCardProxy(
        const string &/*path*/,
        const string &/*service*/) {
      // TODO(benchan): This code conditionally returns a NULL pointer to avoid
      // CellularCapabilityGSM::InitProperties (and thus
      // CellularCapabilityGSM::GetIMSI) from being called during the
      // construction. Remove this workaround after refactoring the tests.
      return test_->create_card_proxy_from_factory_ ?
          test_->card_proxy_.release() : NULL;
    }

    virtual ModemGSMNetworkProxyInterface *CreateModemGSMNetworkProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->network_proxy_.release();
    }

   private:
    CellularCapabilityGSMTest *test_;
  };

  void SetProxy() {
    capability_->proxy_.reset(proxy_.release());
  }

  void SetCardProxy() {
    capability_->card_proxy_.reset(card_proxy_.release());
  }

  void SetNetworkProxy() {
    capability_->network_proxy_.reset(network_proxy_.release());
  }

  void SetAccessTechnology(uint32 technology) {
    capability_->access_technology_ = technology;
  }

  void SetRegistrationState(uint32 state) {
    capability_->registration_state_ = state;
  }

  void SetService() {
    cellular_->service_ = new CellularService(&modem_info_, cellular_);
  }

  void SetupCommonProxiesExpectations() {
    EXPECT_CALL(*proxy_, set_state_changed_callback(_));
    EXPECT_CALL(*network_proxy_, set_signal_quality_callback(_));
    EXPECT_CALL(*network_proxy_, set_network_mode_callback(_));
    EXPECT_CALL(*network_proxy_, set_registration_info_callback(_));
  }

  void SetupCommonStartModemExpectations() {
    SetupCommonProxiesExpectations();

    EXPECT_CALL(*proxy_, Enable(_, _, _, CellularCapability::kTimeoutEnable))
        .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeEnable));
    EXPECT_CALL(*card_proxy_,
                GetIMEI(_, _, CellularCapability::kTimeoutDefault))
        .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeGetIMEI));
    EXPECT_CALL(*card_proxy_,
                GetIMSI(_, _, CellularCapability::kTimeoutDefault))
        .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeGetIMSI));
    EXPECT_CALL(*network_proxy_, AccessTechnology());
    EXPECT_CALL(*card_proxy_, EnabledFacilityLocks());
    EXPECT_CALL(*proxy_,
                GetModemInfo(_, _, CellularCapability::kTimeoutDefault))
        .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeGetModemInfo));
    EXPECT_CALL(*network_proxy_,
                GetRegistrationInfo(_, _, CellularCapability::kTimeoutDefault));
    EXPECT_CALL(*network_proxy_,
                GetSignalQuality(_, _, CellularCapability::kTimeoutDefault));
    EXPECT_CALL(*this, TestCallback(IsSuccess()));
  }

  void InitProxies() {
    AllowCreateCardProxyFromFactory();
    capability_->InitProxies();
  }

  void AllowCreateCardProxyFromFactory() {
    create_card_proxy_from_factory_ = true;
  }

  EventDispatcher dispatcher_;
  MockModemInfo modem_info_;
  bool create_card_proxy_from_factory_;
  scoped_ptr<MockModemProxy> proxy_;
  scoped_ptr<MockModemSimpleProxy> simple_proxy_;
  scoped_ptr<MockModemGSMCardProxy> card_proxy_;
  scoped_ptr<MockModemGSMNetworkProxy> network_proxy_;
  TestProxyFactory proxy_factory_;
  CellularCapabilityGSM *capability_;  // Owned by |cellular_|.
  DeviceMockAdaptor *device_adaptor_;  // Owned by |cellular_|.
  CellularRefPtr cellular_;
  ScanResultsCallback scan_callback_;  // saved for testing scan operations
};

const char CellularCapabilityGSMTest::kAddress[] = "1122334455";
const char CellularCapabilityGSMTest::kTestMobileProviderDBPath[] =
    "provider_db_unittest.bfd";
const char CellularCapabilityGSMTest::kTestCarrier[] = "The Cellular Carrier";
const char CellularCapabilityGSMTest::kTestNetwork[] = "310555";
const char CellularCapabilityGSMTest::kPIN[] = "9876";
const char CellularCapabilityGSMTest::kPUK[] = "8765";
const char CellularCapabilityGSMTest::kIMEI[] = "987654321098765";
const char CellularCapabilityGSMTest::kIMSI[] = "310150123456789";
const char CellularCapabilityGSMTest::kMSISDN[] = "12345678901";
const char CellularCapabilityGSMTest::kScanID0[] = "123";
const char CellularCapabilityGSMTest::kScanID1[] = "456";
const int CellularCapabilityGSMTest::kStrength = 80;

TEST_F(CellularCapabilityGSMTest, PropertyStore) {
  EXPECT_TRUE(cellular_->store().Contains(kSIMLockStatusProperty));
}

TEST_F(CellularCapabilityGSMTest, GetIMEI) {
  EXPECT_CALL(*card_proxy_, GetIMEI(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this,
                       &CellularCapabilityGSMTest::InvokeGetIMEI));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetCardProxy();
  ASSERT_TRUE(capability_->imei_.empty());
  capability_->GetIMEI(Bind(&CellularCapabilityGSMTest::TestCallback,
                            Unretained(this)));
  EXPECT_EQ(kIMEI, capability_->imei_);
}

TEST_F(CellularCapabilityGSMTest, GetIMSI) {
  EXPECT_CALL(*card_proxy_, GetIMSI(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this,
                       &CellularCapabilityGSMTest::InvokeGetIMSI))
      .WillOnce(Invoke(this,
                       &CellularCapabilityGSMTest::InvokeGetIMSI2));
  EXPECT_CALL(*this, TestCallback(IsSuccess())).Times(2);
  SetCardProxy();
  ResultCallback callback = Bind(&CellularCapabilityGSMTest::TestCallback,
                                 Unretained(this));
  EXPECT_TRUE(capability_->imsi_.empty());
  EXPECT_FALSE(capability_->sim_present_);
  capability_->GetIMSI(callback);
  EXPECT_EQ(kIMSI, capability_->imsi_);
  EXPECT_TRUE(capability_->sim_present_);
  capability_->imsi_.clear();
  InitProviderDB();
  capability_->GetIMSI(callback);
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
}

// In this test, the call to the proxy's GetIMSI() will always indicate failure,
// which will cause the retry logic to call the proxy again a number of times.
// Eventually, the retries expire.
TEST_F(CellularCapabilityGSMTest, GetIMSIFails) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(logging::LOG_INFO, StrEq("cellular_capability_gsm.cc"),
                       ::testing::StartsWith("GetIMSI failed - ")));
  EXPECT_CALL(*card_proxy_, GetIMSI(_, _, CellularCapability::kTimeoutDefault))
      .Times(CellularCapabilityGSM::kGetIMSIRetryLimit + 2)
      .WillRepeatedly(Invoke(this,
                             &CellularCapabilityGSMTest::InvokeGetIMSIFails));
  EXPECT_CALL(*this, TestCallback(IsFailure())).Times(2);
  SetCardProxy();
  ResultCallback callback = Bind(&CellularCapabilityGSMTest::TestCallback,
                                 Unretained(this));
  EXPECT_TRUE(capability_->imsi_.empty());
  EXPECT_FALSE(capability_->sim_present_);

  capability_->sim_lock_status_.lock_type = "sim-pin";
  capability_->GetIMSI(callback);
  EXPECT_TRUE(capability_->imsi_.empty());
  EXPECT_TRUE(capability_->sim_present_);

  capability_->sim_lock_status_.lock_type.clear();
  capability_->sim_present_ = false;
  capability_->get_imsi_retries_ = 0;
  EXPECT_EQ(CellularCapabilityGSM::kGetIMSIRetryDelayMilliseconds,
            capability_->get_imsi_retry_delay_milliseconds_);

  // Set the delay to zero to speed up the test.
  capability_->get_imsi_retry_delay_milliseconds_ = 0;
  capability_->GetIMSI(callback);
  for (int i = 0; i < CellularCapabilityGSM::kGetIMSIRetryLimit; ++i) {
    dispatcher_.DispatchPendingEvents();
  }
  EXPECT_EQ(CellularCapabilityGSM::kGetIMSIRetryLimit + 1,
            capability_->get_imsi_retries_);
  EXPECT_TRUE(capability_->imsi_.empty());
  EXPECT_FALSE(capability_->sim_present_);
}

TEST_F(CellularCapabilityGSMTest, GetMSISDN) {
  EXPECT_CALL(*card_proxy_, GetMSISDN(_, _,
                                      CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this,
                       &CellularCapabilityGSMTest::InvokeGetMSISDN));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetCardProxy();
  ASSERT_TRUE(capability_->mdn_.empty());
  capability_->GetMSISDN(Bind(&CellularCapabilityGSMTest::TestCallback,
                            Unretained(this)));
  EXPECT_EQ(kMSISDN, capability_->mdn_);
}

TEST_F(CellularCapabilityGSMTest, GetSPN) {
  EXPECT_CALL(*card_proxy_, GetSPN(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this,
                       &CellularCapabilityGSMTest::InvokeGetSPN));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetCardProxy();
  ASSERT_TRUE(capability_->spn_.empty());
  capability_->GetSPN(Bind(&CellularCapabilityGSMTest::TestCallback,
                            Unretained(this)));
  EXPECT_EQ(kTestCarrier, capability_->spn_);
}

TEST_F(CellularCapabilityGSMTest, GetSignalQuality) {
  EXPECT_CALL(*network_proxy_,
              GetSignalQuality(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this,
                       &CellularCapabilityGSMTest::InvokeGetSignalQuality));
  SetNetworkProxy();
  SetService();
  EXPECT_EQ(0, cellular_->service()->strength());
  capability_->GetSignalQuality();
  EXPECT_EQ(kStrength, cellular_->service()->strength());
}

TEST_F(CellularCapabilityGSMTest, RegisterOnNetwork) {
  EXPECT_CALL(*network_proxy_, Register(kTestNetwork, _, _,
                                        CellularCapability::kTimeoutRegister))
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeRegister));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetNetworkProxy();
  Error error;
  capability_->RegisterOnNetwork(kTestNetwork, &error,
                                 Bind(&CellularCapabilityGSMTest::TestCallback,
                                      Unretained(this)));
  EXPECT_EQ(kTestNetwork, capability_->selected_network_);
}

TEST_F(CellularCapabilityGSMTest, IsRegistered) {
  EXPECT_FALSE(capability_->IsRegistered());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_IDLE);
  EXPECT_FALSE(capability_->IsRegistered());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_HOME);
  EXPECT_TRUE(capability_->IsRegistered());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_SEARCHING);
  EXPECT_FALSE(capability_->IsRegistered());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_DENIED);
  EXPECT_FALSE(capability_->IsRegistered());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_UNKNOWN);
  EXPECT_FALSE(capability_->IsRegistered());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING);
  EXPECT_TRUE(capability_->IsRegistered());
}

TEST_F(CellularCapabilityGSMTest, GetRegistrationState) {
  ASSERT_FALSE(capability_->IsRegistered());
  EXPECT_CALL(*network_proxy_,
              GetRegistrationInfo(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this,
                       &CellularCapabilityGSMTest::InvokeGetRegistrationInfo));
  SetNetworkProxy();
  capability_->GetRegistrationState();
  EXPECT_TRUE(capability_->IsRegistered());
  EXPECT_EQ(MM_MODEM_GSM_NETWORK_REG_STATUS_HOME,
            capability_->registration_state_);
}

TEST_F(CellularCapabilityGSMTest, RequirePIN) {
  EXPECT_CALL(*card_proxy_, EnablePIN(kPIN, true, _, _,
                                      CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeEnablePIN));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetCardProxy();
  Error error;
  capability_->RequirePIN(kPIN, true, &error,
                          Bind(&CellularCapabilityGSMTest::TestCallback,
                               Unretained(this)));
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(CellularCapabilityGSMTest, EnterPIN) {
  EXPECT_CALL(*card_proxy_, SendPIN(kPIN, _, _,
                                    CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeSendPIN));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetCardProxy();
  Error error;
  capability_->EnterPIN(kPIN, &error,
                        Bind(&CellularCapabilityGSMTest::TestCallback,
                             Unretained(this)));
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(CellularCapabilityGSMTest, UnblockPIN) {
  EXPECT_CALL(*card_proxy_, SendPUK(kPUK, kPIN, _, _,
                                    CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeSendPUK));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetCardProxy();
  Error error;
  capability_->UnblockPIN(kPUK, kPIN, &error,
                          Bind(&CellularCapabilityGSMTest::TestCallback,
                             Unretained(this)));
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(CellularCapabilityGSMTest, ChangePIN) {
  static const char kOldPIN[] = "1111";
  EXPECT_CALL(*card_proxy_, ChangePIN(kOldPIN, kPIN, _, _,
                                    CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeChangePIN));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetCardProxy();
  Error error;
  capability_->ChangePIN(kOldPIN, kPIN, &error,
                         Bind(&CellularCapabilityGSMTest::TestCallback,
                             Unretained(this)));
  EXPECT_TRUE(error.IsSuccess());
}

namespace {

MATCHER(SizeIs2, "") {
  return arg.size() == 2;
}

}  // namespace

TEST_F(CellularCapabilityGSMTest, Scan) {
  Error error;
  EXPECT_CALL(*network_proxy_, Scan(_, _, CellularCapability::kTimeoutScan))
      .WillOnce(SaveArg<1>(&scan_callback_));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  capability_->found_networks_.resize(3, Stringmap());
  EXPECT_CALL(*device_adaptor_,
              EmitStringmapsChanged(kFoundNetworksProperty, SizeIs2()));
  EXPECT_CALL(*device_adaptor_, EmitBoolChanged(kScanningProperty, true));
  EXPECT_FALSE(capability_->scanning_);

  SetNetworkProxy();
  capability_->Scan(&error, Bind(&CellularCapabilityGSMTest::TestCallback,
                                 Unretained(this)));
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(capability_->scanning_);

  // Simulate the completion of the scan...
  EXPECT_CALL(*device_adaptor_, EmitBoolChanged(kScanningProperty, false));
  InvokeScanReply();
  EXPECT_FALSE(capability_->scanning_);
  EXPECT_EQ(2, capability_->found_networks_.size());
  EXPECT_EQ(kScanID0, capability_->found_networks_[0][kNetworkIdProperty]);
  EXPECT_EQ(kScanID1, capability_->found_networks_[1][kNetworkIdProperty]);
}

TEST_F(CellularCapabilityGSMTest, ParseScanResult) {
  static const char kID[] = "123";
  static const char kLongName[] = "long name";
  static const char kShortName[] = "short name";
  GSMScanResult result;
  result[CellularCapabilityGSM::kNetworkPropertyStatus] = "1";
  result[CellularCapabilityGSM::kNetworkPropertyID] = kID;
  result[CellularCapabilityGSM::kNetworkPropertyLongName] = kLongName;
  result[CellularCapabilityGSM::kNetworkPropertyShortName] = kShortName;
  result[CellularCapabilityGSM::kNetworkPropertyAccessTechnology] = "3";
  result["unknown property"] = "random value";
  Stringmap parsed = capability_->ParseScanResult(result);
  EXPECT_EQ(5, parsed.size());
  EXPECT_EQ("available", parsed[kStatusProperty]);
  EXPECT_EQ(kID, parsed[kNetworkIdProperty]);
  EXPECT_EQ(kLongName, parsed[kLongNameProperty]);
  EXPECT_EQ(kShortName, parsed[kShortNameProperty]);
  EXPECT_EQ(kNetworkTechnologyEdge, parsed[kTechnologyProperty]);
}

TEST_F(CellularCapabilityGSMTest, ParseScanResultProviderLookup) {
  InitProviderDB();
  static const char kID[] = "310210";
  GSMScanResult result;
  result[CellularCapabilityGSM::kNetworkPropertyID] = kID;
  Stringmap parsed = capability_->ParseScanResult(result);
  EXPECT_EQ(2, parsed.size());
  EXPECT_EQ(kID, parsed[kNetworkIdProperty]);
  EXPECT_EQ("T-Mobile", parsed[kLongNameProperty]);
}

TEST_F(CellularCapabilityGSMTest, SetAccessTechnology) {
  capability_->SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GSM);
  EXPECT_EQ(MM_MODEM_GSM_ACCESS_TECH_GSM, capability_->access_technology_);
  SetService();
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_HOME);
  capability_->SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GPRS);
  EXPECT_EQ(MM_MODEM_GSM_ACCESS_TECH_GPRS, capability_->access_technology_);
  EXPECT_EQ(kNetworkTechnologyGprs, cellular_->service()->network_technology());
}

TEST_F(CellularCapabilityGSMTest, UpdateOperatorInfo) {
  static const char kOperatorName[] = "Swisscom";
  InitProviderDB();
  capability_->serving_operator_.SetCode("22801");
  SetService();
  capability_->UpdateOperatorInfo();
  EXPECT_EQ(kOperatorName, capability_->serving_operator_.GetName());
  EXPECT_EQ("ch", capability_->serving_operator_.GetCountry());
  EXPECT_EQ(kOperatorName, cellular_->service()->serving_operator().GetName());

  static const char kTestOperator[] = "Testcom";
  capability_->serving_operator_.SetName(kTestOperator);
  capability_->serving_operator_.SetCountry("");
  capability_->UpdateOperatorInfo();
  EXPECT_EQ(kTestOperator, capability_->serving_operator_.GetName());
  EXPECT_EQ("ch", capability_->serving_operator_.GetCountry());
  EXPECT_EQ(kTestOperator, cellular_->service()->serving_operator().GetName());
}

TEST_F(CellularCapabilityGSMTest, UpdateStatus) {
  InitProviderDB();
  DBusPropertiesMap props;
  capability_->imsi_ = "310240123456789";
  props[CellularCapability::kModemPropertyIMSI].writer().append_string("");
  capability_->UpdateStatus(props);
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
}

TEST_F(CellularCapabilityGSMTest, AllowRoaming) {
  EXPECT_FALSE(cellular_->allow_roaming_);
  EXPECT_FALSE(capability_->provider_requires_roaming_);
  EXPECT_FALSE(capability_->AllowRoaming());
  capability_->provider_requires_roaming_ = true;
  EXPECT_TRUE(capability_->AllowRoaming());
  capability_->provider_requires_roaming_ = false;
  cellular_->allow_roaming_ = true;
  EXPECT_TRUE(capability_->AllowRoaming());
}

TEST_F(CellularCapabilityGSMTest, SetHomeProvider) {
  static const char kCountry[] = "us";
  static const char kCode[] = "310160";
  capability_->imsi_ = "310240123456789";

  EXPECT_FALSE(capability_->home_provider_);
  EXPECT_FALSE(capability_->provider_requires_roaming_);

  capability_->SetHomeProvider();  // No mobile provider DB available.
  EXPECT_TRUE(cellular_->home_provider().GetName().empty());
  EXPECT_TRUE(cellular_->home_provider().GetCountry().empty());
  EXPECT_TRUE(cellular_->home_provider().GetCode().empty());
  EXPECT_FALSE(capability_->provider_requires_roaming_);

  InitProviderDB();
  capability_->SetHomeProvider();
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
  EXPECT_EQ(kCountry, cellular_->home_provider().GetCountry());
  EXPECT_EQ(kCode, cellular_->home_provider().GetCode());
  EXPECT_EQ(4, capability_->apn_list_.size());
  ASSERT_TRUE(capability_->home_provider_);
  EXPECT_FALSE(capability_->provider_requires_roaming_);

  Cellular::Operator oper;
  cellular_->set_home_provider(oper);
  capability_->spn_ = kTestCarrier;
  capability_->SetHomeProvider();
  EXPECT_EQ(kTestCarrier, cellular_->home_provider().GetName());
  EXPECT_EQ(kCountry, cellular_->home_provider().GetCountry());
  EXPECT_EQ(kCode, cellular_->home_provider().GetCode());
  EXPECT_FALSE(capability_->provider_requires_roaming_);

  static const char kCubic[] = "Cubic";
  capability_->spn_ = kCubic;
  capability_->SetHomeProvider();
  EXPECT_EQ(kCubic, cellular_->home_provider().GetName());
  EXPECT_EQ("", cellular_->home_provider().GetCode());
  ASSERT_TRUE(capability_->home_provider_);
  EXPECT_TRUE(capability_->provider_requires_roaming_);

  static const char kCUBIC[] = "CUBIC";
  capability_->spn_ = kCUBIC;
  capability_->home_provider_ = NULL;
  capability_->SetHomeProvider();
  EXPECT_EQ(kCUBIC, cellular_->home_provider().GetName());
  EXPECT_EQ("", cellular_->home_provider().GetCode());
  ASSERT_TRUE(capability_->home_provider_);
  EXPECT_TRUE(capability_->provider_requires_roaming_);
}

namespace {

MATCHER(SizeIs4, "") {
  return arg.size() == 4;
}

}  // namespace

TEST_F(CellularCapabilityGSMTest, InitAPNList) {
  InitProviderDB();
  capability_->home_provider_ =
      mobile_provider_lookup_by_name(modem_info_.provider_db(), "T-Mobile");
  ASSERT_TRUE(capability_->home_provider_);
  EXPECT_EQ(0, capability_->apn_list_.size());
  EXPECT_CALL(*device_adaptor_,
              EmitStringmapsChanged(kCellularApnListProperty, SizeIs4()));
  capability_->InitAPNList();
  EXPECT_EQ(4, capability_->apn_list_.size());
  EXPECT_EQ("wap.voicestream.com",
            capability_->apn_list_[1][kApnProperty]);
  EXPECT_EQ("Web2Go/t-zones",
            capability_->apn_list_[1][kApnNameProperty]);
}

TEST_F(CellularCapabilityGSMTest, GetNetworkTechnologyString) {
  EXPECT_EQ("", capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GSM);
  EXPECT_EQ(kNetworkTechnologyGsm, capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GSM_COMPACT);
  EXPECT_EQ(kNetworkTechnologyGsm, capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GPRS);
  EXPECT_EQ(kNetworkTechnologyGprs, capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_EDGE);
  EXPECT_EQ(kNetworkTechnologyEdge, capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_UMTS);
  EXPECT_EQ(kNetworkTechnologyUmts, capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSDPA);
  EXPECT_EQ(kNetworkTechnologyHspa, capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSUPA);
  EXPECT_EQ(kNetworkTechnologyHspa, capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSPA);
  EXPECT_EQ(kNetworkTechnologyHspa, capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSPA_PLUS);
  EXPECT_EQ(kNetworkTechnologyHspaPlus,
            capability_->GetNetworkTechnologyString());
}

TEST_F(CellularCapabilityGSMTest, GetRoamingStateString) {
  EXPECT_EQ(kRoamingStateUnknown, capability_->GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_HOME);
  EXPECT_EQ(kRoamingStateHome, capability_->GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING);
  EXPECT_EQ(kRoamingStateRoaming, capability_->GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_SEARCHING);
  EXPECT_EQ(kRoamingStateUnknown, capability_->GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_DENIED);
  EXPECT_EQ(kRoamingStateUnknown, capability_->GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_IDLE);
  EXPECT_EQ(kRoamingStateUnknown, capability_->GetRoamingStateString());
}

TEST_F(CellularCapabilityGSMTest, CreateFriendlyServiceName) {
  CellularCapabilityGSM::friendly_service_name_id_ = 0;
  EXPECT_EQ("GSMNetwork0", capability_->CreateFriendlyServiceName());
  EXPECT_EQ("GSMNetwork1", capability_->CreateFriendlyServiceName());

  capability_->serving_operator_.SetCode("1234");
  EXPECT_EQ("cellular_1234", capability_->CreateFriendlyServiceName());

  static const char kTestCarrier[] = "A GSM Carrier";
  capability_->carrier_ = kTestCarrier;
  EXPECT_EQ(kTestCarrier, capability_->CreateFriendlyServiceName());

  static const char kHomeProvider[] = "The GSM Home Provider";
  cellular_->home_provider_.SetName(kHomeProvider);
  EXPECT_EQ(kTestCarrier, capability_->CreateFriendlyServiceName());
  capability_->registration_state_ = MM_MODEM_GSM_NETWORK_REG_STATUS_HOME;
  EXPECT_EQ(kHomeProvider, capability_->CreateFriendlyServiceName());

  static const char kTestOperator[] = "A GSM Operator";
  capability_->serving_operator_.SetName(kTestOperator);
  EXPECT_EQ(kTestOperator, capability_->CreateFriendlyServiceName());

  capability_->registration_state_ = MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING;
  EXPECT_EQ(StringPrintf("%s | %s", kHomeProvider, kTestOperator),
            capability_->CreateFriendlyServiceName());
}

TEST_F(CellularCapabilityGSMTest, SetStorageIdentifier) {
  SetService();
  capability_->OnServiceCreated();
  EXPECT_EQ(string(kTypeCellular) + "_" + kAddress + "_" +
            cellular_->service()->friendly_name(),
            cellular_->service()->GetStorageIdentifier());
  capability_->imsi_ = kIMSI;
  capability_->OnServiceCreated();
  EXPECT_EQ(string(kTypeCellular) + "_" + kAddress + "_" + kIMSI,
            cellular_->service()->GetStorageIdentifier());
}

MATCHER_P(KeyValueStoreEq, value, "") {
  bool match = value.bool_properties() == arg.bool_properties() &&
      value.int_properties() == arg.int_properties() &&
      value.string_properties() == arg.string_properties() &&
      value.uint_properties() == arg.uint_properties();
  if (!match) {
    *result_listener << "\nExpected KeyValueStore:\n"
                     << "\tbool_properties: "
                     << testing::PrintToString(value.bool_properties())
                     << "\n\tint_properties: "
                     << testing::PrintToString(value.int_properties())
                     << "\n\tstring_properties: "
                     << testing::PrintToString(value.string_properties())
                     << "\n\tint_properties: "
                     << testing::PrintToString(value.uint_properties())
                     << "\nGot KeyValueStore:\n"
                     << "\tbool_properties: "
                     << testing::PrintToString(arg.bool_properties())
                     << "\n\tint_properties: "
                     << testing::PrintToString(arg.int_properties())
                     << "\n\tstring_properties: "
                     << testing::PrintToString(arg.string_properties())
                     << "\n\tuint_properties: "
                     << testing::PrintToString(arg.uint_properties());
  }
  return match;
}

TEST_F(CellularCapabilityGSMTest, OnDBusPropertiesChanged) {
  EXPECT_EQ(MM_MODEM_GSM_ACCESS_TECH_UNKNOWN, capability_->access_technology_);
  EXPECT_FALSE(capability_->sim_lock_status_.enabled);
  EXPECT_EQ("", capability_->sim_lock_status_.lock_type);
  EXPECT_EQ(0, capability_->sim_lock_status_.retries_left);
  DBusPropertiesMap props;
  static const char kLockType[] = "sim-pin";
  const int kRetries = 3;
  props[CellularCapabilityGSM::kPropertyAccessTechnology].writer().
      append_uint32(MM_MODEM_GSM_ACCESS_TECH_EDGE);
  props[CellularCapabilityGSM::kPropertyEnabledFacilityLocks].writer().
      append_uint32(MM_MODEM_GSM_FACILITY_SIM);
  props[CellularCapabilityGSM::kPropertyUnlockRequired].writer().append_string(
      kLockType);
  props[CellularCapabilityGSM::kPropertyUnlockRetries].writer().append_uint32(
      kRetries);
  // Call with the 'wrong' interface and nothing should change.
  capability_->OnDBusPropertiesChanged(MM_MODEM_GSM_INTERFACE, props,
                                       vector<string>());
  EXPECT_EQ(MM_MODEM_GSM_ACCESS_TECH_UNKNOWN, capability_->access_technology_);
  EXPECT_FALSE(capability_->sim_lock_status_.enabled);
  EXPECT_EQ("", capability_->sim_lock_status_.lock_type);
  EXPECT_EQ(0, capability_->sim_lock_status_.retries_left);

  // Call with the MM_MODEM_GSM_NETWORK_INTERFACE interface and expect a change
  // to the enabled state of the SIM lock.
  KeyValueStore lock_status;
  lock_status.SetBool(kSIMLockEnabledProperty, true);
  lock_status.SetString(kSIMLockTypeProperty, "");
  lock_status.SetUint(kSIMLockRetriesLeftProperty, 0);

  EXPECT_CALL(*device_adaptor_, EmitKeyValueStoreChanged(
      kSIMLockStatusProperty,
      KeyValueStoreEq(lock_status)));

  capability_->OnDBusPropertiesChanged(MM_MODEM_GSM_NETWORK_INTERFACE, props,
                                       vector<string>());
  EXPECT_EQ(MM_MODEM_GSM_ACCESS_TECH_EDGE, capability_->access_technology_);
  capability_->OnDBusPropertiesChanged(MM_MODEM_GSM_CARD_INTERFACE, props,
                                       vector<string>());
  EXPECT_TRUE(capability_->sim_lock_status_.enabled);
  EXPECT_TRUE(capability_->sim_lock_status_.lock_type.empty());
  EXPECT_EQ(0, capability_->sim_lock_status_.retries_left);

  // Some properties are sent on the MM_MODEM_INTERFACE.
  capability_->sim_lock_status_.enabled = false;
  capability_->sim_lock_status_.lock_type = "";
  capability_->sim_lock_status_.retries_left = 0;
  KeyValueStore lock_status2;
  lock_status2.SetBool(kSIMLockEnabledProperty, false);
  lock_status2.SetString(kSIMLockTypeProperty, kLockType);
  lock_status2.SetUint(kSIMLockRetriesLeftProperty, kRetries);
  EXPECT_CALL(*device_adaptor_,
              EmitKeyValueStoreChanged(kSIMLockStatusProperty,
                                       KeyValueStoreEq(lock_status2)));
  capability_->OnDBusPropertiesChanged(MM_MODEM_INTERFACE, props,
                                       vector<string>());
  EXPECT_FALSE(capability_->sim_lock_status_.enabled);
  EXPECT_EQ(kLockType, capability_->sim_lock_status_.lock_type);
  EXPECT_EQ(kRetries, capability_->sim_lock_status_.retries_left);
}

TEST_F(CellularCapabilityGSMTest, SetupApnTryList) {
  static const string kTmobileApn("epc.tmobile.com");
  static const string kLastGoodApn("remembered.apn");
  static const string kLastGoodUsername("remembered.user");
  static const string kSuppliedApn("my.apn");

  SetService();
  capability_->imsi_ = "310240123456789";
  InitProviderDB();
  capability_->SetHomeProvider();
  DBusPropertiesMap props;
  capability_->SetupConnectProperties(&props);
  EXPECT_FALSE(props.find(kApnProperty) == props.end());
  EXPECT_EQ(kTmobileApn, props[kApnProperty].reader().get_string());

  ProfileRefPtr profile(new NiceMock<MockProfile>(
      modem_info_.control_interface(), modem_info_.metrics(),
      modem_info_.manager()));
  cellular_->service()->set_profile(profile);
  Stringmap apn_info;
  apn_info[kApnProperty] = kLastGoodApn;
  apn_info[kApnUsernameProperty] = kLastGoodUsername;
  cellular_->service()->SetLastGoodApn(apn_info);
  props.clear();
  EXPECT_TRUE(props.find(kApnProperty) == props.end());
  capability_->SetupConnectProperties(&props);
  // We expect the list to contain the last good APN, plus
  // the 4 APNs from the mobile provider info database.
  EXPECT_EQ(5, capability_->apn_try_list_.size());
  EXPECT_FALSE(props.find(kApnProperty) == props.end());
  EXPECT_EQ(kLastGoodApn, props[kApnProperty].reader().get_string());
  EXPECT_FALSE(props.find(kApnUsernameProperty) == props.end());
  EXPECT_EQ(kLastGoodUsername,
            props[kApnUsernameProperty].reader().get_string());

  Error error;
  apn_info.clear();
  props.clear();
  apn_info[kApnProperty] = kSuppliedApn;
  // Setting the APN has the side effect of clearing the LastGoodApn,
  // so the try list will have 5 elements, with the first one being
  // the supplied APN.
  cellular_->service()->SetApn(apn_info, &error);
  EXPECT_TRUE(props.find(kApnProperty) == props.end());
  capability_->SetupConnectProperties(&props);
  EXPECT_EQ(5, capability_->apn_try_list_.size());
  EXPECT_FALSE(props.find(kApnProperty) == props.end());
  EXPECT_EQ(kSuppliedApn, props[kApnProperty].reader().get_string());

  apn_info.clear();
  props.clear();
  apn_info[kApnProperty] = kLastGoodApn;
  apn_info[kApnUsernameProperty] = kLastGoodUsername;
  // Now when LastGoodAPN is set, it will be the one selected.
  cellular_->service()->SetLastGoodApn(apn_info);
  EXPECT_TRUE(props.find(kApnProperty) == props.end());
  capability_->SetupConnectProperties(&props);
  // We expect the list to contain the last good APN, plus
  // the user-supplied APN, plus the 4 APNs from the mobile
  // provider info database.
  EXPECT_EQ(6, capability_->apn_try_list_.size());
  EXPECT_FALSE(props.find(kApnProperty) == props.end());
  EXPECT_EQ(kLastGoodApn, props[kApnProperty].reader().get_string());
  EXPECT_FALSE(props.find(kApnUsernameProperty) == props.end());
  EXPECT_EQ(kLastGoodUsername,
            props[kApnUsernameProperty].reader().get_string());
}

TEST_F(CellularCapabilityGSMTest, StartModemSuccess) {
  SetupCommonStartModemExpectations();
  EXPECT_CALL(*card_proxy_,
              GetSPN(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeGetSPN));
  EXPECT_CALL(*card_proxy_,
              GetMSISDN(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeGetMSISDN));
  AllowCreateCardProxyFromFactory();

  Error error;
  capability_->StartModem(
      &error, Bind(&CellularCapabilityGSMTest::TestCallback, Unretained(this)));
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, StartModemGetSPNFail) {
  SetupCommonStartModemExpectations();
  EXPECT_CALL(*card_proxy_,
              GetSPN(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeGetSPNFail));
  EXPECT_CALL(*card_proxy_,
              GetMSISDN(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeGetMSISDN));
  AllowCreateCardProxyFromFactory();

  Error error;
  capability_->StartModem(
      &error, Bind(&CellularCapabilityGSMTest::TestCallback, Unretained(this)));
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, StartModemGetMSISDNFail) {
  SetupCommonStartModemExpectations();
  EXPECT_CALL(*card_proxy_,
              GetSPN(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeGetSPN));
  EXPECT_CALL(*card_proxy_,
              GetMSISDN(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeGetMSISDNFail));
  AllowCreateCardProxyFromFactory();

  Error error;
  capability_->StartModem(
      &error, Bind(&CellularCapabilityGSMTest::TestCallback, Unretained(this)));
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, ConnectFailureNoService) {
  // Make sure we don't crash if the connect failed and there is no
  // CellularService object.  This can happen if the modem is enabled and
  // then quickly disabled.
  SetupCommonProxiesExpectations();
  EXPECT_CALL(*simple_proxy_,
              Connect(_, _, _, CellularCapabilityGSM::kTimeoutConnect))
       .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeConnectFail));
  EXPECT_CALL(*this, TestCallback(IsFailure()));
  InitProxies();
  EXPECT_FALSE(capability_->cellular()->service());
  Error error;
  DBusPropertiesMap props;
  capability_->Connect(props, &error,
                       Bind(&CellularCapabilityGSMTest::TestCallback,
                            Unretained(this)));
}

}  // namespace shill
