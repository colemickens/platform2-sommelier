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
#include "shill/mock_metrics.h"
#include "shill/mock_modem_gsm_card_proxy.h"
#include "shill/mock_modem_gsm_network_proxy.h"
#include "shill/mock_profile.h"
#include "shill/nice_mock_control.h"

using base::Bind;
using base::Unretained;
using std::string;
using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace shill {

MATCHER(IsSuccess, "") {
  return arg.IsSuccess();
}

class CellularCapabilityGSMTest : public testing::Test {
 public:
  CellularCapabilityGSMTest()
      : cellular_(new Cellular(&control_,
                               &dispatcher_,
                               &metrics_,
                               NULL,
                               "",
                               kAddress,
                               0,
                               Cellular::kTypeGSM,
                               "",
                               "",
                               NULL)),
        card_proxy_(new MockModemGSMCardProxy()),
        network_proxy_(new MockModemGSMNetworkProxy()),
        capability_(NULL),
        device_adaptor_(NULL),
        provider_db_(NULL) {}

  virtual ~CellularCapabilityGSMTest() {
    cellular_->service_ = NULL;
    mobile_provider_close_db(provider_db_);
    provider_db_ = NULL;
    capability_ = NULL;
    device_adaptor_ = NULL;
  }

  virtual void SetUp() {
    capability_ =
        dynamic_cast<CellularCapabilityGSM *>(cellular_->capability_.get());
    device_adaptor_ =
        dynamic_cast<NiceMock<DeviceMockAdaptor> *>(cellular_->adaptor());
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
  void InvokeGetMSISDN(Error *error, const GSMIdentifierCallback &callback,
                       int timeout) {
    callback.Run(kMSISDN, Error());
  }
  void InvokeGetSPN(Error *error, const GSMIdentifierCallback &callback,
                    int timeout) {
    callback.Run(kTestCarrier, Error());
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
  void InvokeScan(Error *error, const ScanResultsCallback &callback,
                  int timeout) {
    GSMScanResults results;
    results.push_back(GSMScanResult());
    results[0][CellularCapabilityGSM::kNetworkPropertyID] = kScanID0;
    results.push_back(GSMScanResult());
    results[1][CellularCapabilityGSM::kNetworkPropertyID] = kScanID1;
    callback.Run(results, Error());
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
    cellular_->service_ = new CellularService(
        &control_, &dispatcher_, &metrics_, NULL, cellular_);
  }

  void InitProviderDB() {
    provider_db_ = mobile_provider_open_db(kTestMobileProviderDBPath);
    ASSERT_TRUE(provider_db_);
    cellular_->provider_db_ = provider_db_;
  }

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  CellularRefPtr cellular_;
  scoped_ptr<MockModemGSMCardProxy> card_proxy_;
  scoped_ptr<MockModemGSMNetworkProxy> network_proxy_;
  CellularCapabilityGSM *capability_;  // Owned by |cellular_|.
  NiceMock<DeviceMockAdaptor> *device_adaptor_;  // Owned by |cellular_|.
  mobile_provider_db *provider_db_;
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
  EXPECT_TRUE(cellular_->store().Contains(flimflam::kSIMLockStatusProperty));
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
  ASSERT_TRUE(capability_->imsi_.empty());
  capability_->GetIMSI(callback);
  EXPECT_EQ(kIMSI, capability_->imsi_);
  capability_->imsi_.clear();
  InitProviderDB();
  capability_->GetIMSI(callback);
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
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
      .WillOnce(Invoke(this, &CellularCapabilityGSMTest::InvokeScan));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  capability_->found_networks_.resize(3, Stringmap());
  EXPECT_CALL(*device_adaptor_,
              EmitStringmapsChanged(flimflam::kFoundNetworksProperty,
                                    SizeIs2()));
  SetNetworkProxy();
  capability_->Scan(&error, Bind(&CellularCapabilityGSMTest::TestCallback,
                                 Unretained(this)));
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(2, capability_->found_networks_.size());
  EXPECT_EQ(kScanID0,
            capability_->found_networks_[0][flimflam::kNetworkIdProperty]);
  EXPECT_EQ(kScanID1,
            capability_->found_networks_[1][flimflam::kNetworkIdProperty]);
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
  EXPECT_EQ("available", parsed[flimflam::kStatusProperty]);
  EXPECT_EQ(kID, parsed[flimflam::kNetworkIdProperty]);
  EXPECT_EQ(kLongName, parsed[flimflam::kLongNameProperty]);
  EXPECT_EQ(kShortName, parsed[flimflam::kShortNameProperty]);
  EXPECT_EQ(flimflam::kNetworkTechnologyEdge,
            parsed[flimflam::kTechnologyProperty]);
}

TEST_F(CellularCapabilityGSMTest, ParseScanResultProviderLookup) {
  InitProviderDB();
  static const char kID[] = "310210";
  GSMScanResult result;
  result[CellularCapabilityGSM::kNetworkPropertyID] = kID;
  Stringmap parsed = capability_->ParseScanResult(result);
  EXPECT_EQ(2, parsed.size());
  EXPECT_EQ(kID, parsed[flimflam::kNetworkIdProperty]);
  EXPECT_EQ("T-Mobile", parsed[flimflam::kLongNameProperty]);
}

TEST_F(CellularCapabilityGSMTest, SetAccessTechnology) {
  capability_->SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GSM);
  EXPECT_EQ(MM_MODEM_GSM_ACCESS_TECH_GSM, capability_->access_technology_);
  SetService();
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_HOME);
  capability_->SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GPRS);
  EXPECT_EQ(MM_MODEM_GSM_ACCESS_TECH_GPRS, capability_->access_technology_);
  EXPECT_EQ(flimflam::kNetworkTechnologyGprs,
            cellular_->service()->network_technology());
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
}

TEST_F(CellularCapabilityGSMTest, UpdateStatus) {
  InitProviderDB();
  DBusPropertiesMap props;
  capability_->imsi_ = "310240123456789";
  props[CellularCapability::kPropertyIMSI].writer().append_string("");
  capability_->UpdateStatus(props);
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
}

TEST_F(CellularCapabilityGSMTest, SetHomeProvider) {
  static const char kCountry[] = "us";
  static const char kCode[] = "310160";
  capability_->imsi_ = "310240123456789";

  capability_->SetHomeProvider();  // No mobile provider DB available.
  EXPECT_TRUE(cellular_->home_provider().GetName().empty());
  EXPECT_TRUE(cellular_->home_provider().GetCountry().empty());
  EXPECT_TRUE(cellular_->home_provider().GetCode().empty());

  InitProviderDB();
  capability_->SetHomeProvider();
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
  EXPECT_EQ(kCountry, cellular_->home_provider().GetCountry());
  EXPECT_EQ(kCode, cellular_->home_provider().GetCode());
  EXPECT_EQ(4, capability_->apn_list_.size());

  Cellular::Operator oper;
  cellular_->set_home_provider(oper);
  capability_->spn_ = kTestCarrier;
  capability_->SetHomeProvider();
  EXPECT_EQ(kTestCarrier, cellular_->home_provider().GetName());
  EXPECT_EQ(kCountry, cellular_->home_provider().GetCountry());
  EXPECT_EQ(kCode, cellular_->home_provider().GetCode());
}

namespace {

MATCHER(SizeIs4, "") {
  return arg.size() == 4;
}

}  // namespace

TEST_F(CellularCapabilityGSMTest, InitAPNList) {
  InitProviderDB();
  capability_->home_provider_ =
      mobile_provider_lookup_by_name(cellular_->provider_db(), "T-Mobile");
  ASSERT_TRUE(capability_->home_provider_);
  EXPECT_EQ(0, capability_->apn_list_.size());
  EXPECT_CALL(*device_adaptor_,
              EmitStringmapsChanged(flimflam::kCellularApnListProperty,
                                    SizeIs4()));
  capability_->InitAPNList();
  EXPECT_EQ(4, capability_->apn_list_.size());
  EXPECT_EQ("wap.voicestream.com",
            capability_->apn_list_[1][flimflam::kApnProperty]);
  EXPECT_EQ("Web2Go/t-zones",
            capability_->apn_list_[1][flimflam::kApnNameProperty]);
}

TEST_F(CellularCapabilityGSMTest, GetNetworkTechnologyString) {
  EXPECT_EQ("", capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GSM);
  EXPECT_EQ(flimflam::kNetworkTechnologyGsm,
            capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GSM_COMPACT);
  EXPECT_EQ(flimflam::kNetworkTechnologyGsm,
            capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GPRS);
  EXPECT_EQ(flimflam::kNetworkTechnologyGprs,
            capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_EDGE);
  EXPECT_EQ(flimflam::kNetworkTechnologyEdge,
            capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_UMTS);
  EXPECT_EQ(flimflam::kNetworkTechnologyUmts,
            capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSDPA);
  EXPECT_EQ(flimflam::kNetworkTechnologyHspa,
            capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSUPA);
  EXPECT_EQ(flimflam::kNetworkTechnologyHspa,
            capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSPA);
  EXPECT_EQ(flimflam::kNetworkTechnologyHspa,
            capability_->GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSPA_PLUS);
  EXPECT_EQ(flimflam::kNetworkTechnologyHspaPlus,
            capability_->GetNetworkTechnologyString());
}

TEST_F(CellularCapabilityGSMTest, GetRoamingStateString) {
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_->GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_HOME);
  EXPECT_EQ(flimflam::kRoamingStateHome,
            capability_->GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING);
  EXPECT_EQ(flimflam::kRoamingStateRoaming,
            capability_->GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_SEARCHING);
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_->GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_DENIED);
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_->GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_IDLE);
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_->GetRoamingStateString());
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
  static const char kTestOperator[] = "A GSM Operator";
  capability_->serving_operator_.SetName(kTestOperator);
  EXPECT_EQ(kTestOperator, capability_->CreateFriendlyServiceName());
  static const char kHomeProvider[] = "The GSM Home Provider";
  cellular_->home_provider_.SetName(kHomeProvider);
  EXPECT_EQ(kTestOperator, capability_->CreateFriendlyServiceName());
  capability_->registration_state_ = MM_MODEM_GSM_NETWORK_REG_STATUS_HOME;
  EXPECT_EQ(kHomeProvider, capability_->CreateFriendlyServiceName());
}

TEST_F(CellularCapabilityGSMTest, SetStorageIdentifier) {
  SetService();
  capability_->OnServiceCreated();
  EXPECT_EQ(string(flimflam::kTypeCellular) + "_" + kAddress + "_" +
            cellular_->service()->friendly_name(),
            cellular_->service()->GetStorageIdentifier());
  capability_->imsi_ = kIMSI;
  capability_->OnServiceCreated();
  EXPECT_EQ(string(flimflam::kTypeCellular) + "_" + kAddress + "_" + kIMSI,
            cellular_->service()->GetStorageIdentifier());
}

TEST_F(CellularCapabilityGSMTest, OnModemManagerPropertiesChanged) {
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
  EXPECT_CALL(*device_adaptor_,
              EmitKeyValueStoreChanged(flimflam::kSIMLockStatusProperty, _));
  capability_->OnModemManagerPropertiesChanged(props);
  EXPECT_EQ(MM_MODEM_GSM_ACCESS_TECH_EDGE, capability_->access_technology_);
  EXPECT_TRUE(capability_->sim_lock_status_.enabled);
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
  EXPECT_FALSE(props.find(flimflam::kApnProperty) == props.end());
  EXPECT_EQ(kTmobileApn, props[flimflam::kApnProperty].reader().get_string());

  ProfileRefPtr profile(new NiceMock<MockProfile>(
      &control_, reinterpret_cast<Manager *>(NULL)));
  cellular_->service()->set_profile(profile);
  Stringmap apn_info;
  apn_info[flimflam::kApnProperty] = kLastGoodApn;
  apn_info[flimflam::kApnUsernameProperty] = kLastGoodUsername;
  cellular_->service()->SetLastGoodApn(apn_info);
  props.clear();
  EXPECT_TRUE(props.find(flimflam::kApnProperty) == props.end());
  capability_->SetupConnectProperties(&props);
  // We expect the list to contain the last good APN, plus
  // the 4 APNs from the mobile provider info database.
  EXPECT_EQ(5, capability_->apn_try_list_.size());
  EXPECT_FALSE(props.find(flimflam::kApnProperty) == props.end());
  EXPECT_EQ(kLastGoodApn, props[flimflam::kApnProperty].reader().get_string());
  EXPECT_FALSE(props.find(flimflam::kApnUsernameProperty) == props.end());
  EXPECT_EQ(kLastGoodUsername,
            props[flimflam::kApnUsernameProperty].reader().get_string());

  Error error;
  apn_info.clear();
  props.clear();
  apn_info[flimflam::kApnProperty] = kSuppliedApn;
  // Setting the APN has the side effect of clearing the LastGoodApn,
  // so the try list will have 5 elements, with the first one being
  // the supplied APN.
  cellular_->service()->SetApn(apn_info, &error);
  EXPECT_TRUE(props.find(flimflam::kApnProperty) == props.end());
  capability_->SetupConnectProperties(&props);
  EXPECT_EQ(5, capability_->apn_try_list_.size());
  EXPECT_FALSE(props.find(flimflam::kApnProperty) == props.end());
  EXPECT_EQ(kSuppliedApn, props[flimflam::kApnProperty].reader().get_string());

  apn_info.clear();
  props.clear();
  apn_info[flimflam::kApnProperty] = kLastGoodApn;
  apn_info[flimflam::kApnUsernameProperty] = kLastGoodUsername;
  // Now when LastGoodAPN is set, it will be the one selected.
  cellular_->service()->SetLastGoodApn(apn_info);
  EXPECT_TRUE(props.find(flimflam::kApnProperty) == props.end());
  capability_->SetupConnectProperties(&props);
  // We expect the list to contain the last good APN, plus
  // the user-supplied APN, plus the 4 APNs from the mobile
  // provider info database.
  EXPECT_EQ(6, capability_->apn_try_list_.size());
  EXPECT_FALSE(props.find(flimflam::kApnProperty) == props.end());
  EXPECT_EQ(kLastGoodApn, props[flimflam::kApnProperty].reader().get_string());
  EXPECT_FALSE(props.find(flimflam::kApnUsernameProperty) == props.end());
  EXPECT_EQ(kLastGoodUsername,
            props[flimflam::kApnUsernameProperty].reader().get_string());
}

}  // namespace shill
