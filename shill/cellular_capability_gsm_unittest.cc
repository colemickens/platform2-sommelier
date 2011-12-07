// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_modem_gsm_card_proxy.h"
#include "shill/mock_modem_gsm_network_proxy.h"
#include "shill/nice_mock_control.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace shill {

class CellularCapabilityGSMTest : public testing::Test {
 public:
  CellularCapabilityGSMTest()
      : cellular_(new Cellular(&control_,
                               &dispatcher_,
                               NULL,
                               "",
                               "",
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

 protected:
  static const char kTestMobileProviderDBPath[];
  static const char kTestCarrier[];
  static const char kPIN[];
  static const char kPUK[];
  static const char kIMEI[];
  static const char kIMSI[];
  static const char kMSISDN[];

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
        &control_, &dispatcher_, NULL, cellular_);
  }

  void InitProviderDB() {
    provider_db_ = mobile_provider_open_db(kTestMobileProviderDBPath);
    ASSERT_TRUE(provider_db_);
    cellular_->provider_db_ = provider_db_;
  }

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  CellularRefPtr cellular_;
  scoped_ptr<MockModemGSMCardProxy> card_proxy_;
  scoped_ptr<MockModemGSMNetworkProxy> network_proxy_;
  CellularCapabilityGSM *capability_;  // Owned by |cellular_|.
  NiceMock<DeviceMockAdaptor> *device_adaptor_;  // Owned by |cellular_|.
  mobile_provider_db *provider_db_;
};

const char CellularCapabilityGSMTest::kTestMobileProviderDBPath[] =
    "provider_db_unittest.bfd";
const char CellularCapabilityGSMTest::kTestCarrier[] = "The Cellular Carrier";
const char CellularCapabilityGSMTest::kPIN[] = "9876";
const char CellularCapabilityGSMTest::kPUK[] = "8765";
const char CellularCapabilityGSMTest::kIMEI[] = "987654321098765";
const char CellularCapabilityGSMTest::kIMSI[] = "310150123456789";
const char CellularCapabilityGSMTest::kMSISDN[] = "12345678901";

TEST_F(CellularCapabilityGSMTest, PropertyStore) {
  EXPECT_TRUE(cellular_->store().Contains(flimflam::kSIMLockStatusProperty));
}

TEST_F(CellularCapabilityGSMTest, GetIdentifiers) {
  EXPECT_CALL(*card_proxy_, GetIMEI()).WillOnce(Return(kIMEI));
  EXPECT_CALL(*card_proxy_, GetIMSI()).WillOnce(Return(kIMSI));
  EXPECT_CALL(*card_proxy_, GetSPN()).WillOnce(Return(kTestCarrier));
  EXPECT_CALL(*card_proxy_, GetMSISDN()).WillOnce(Return(kMSISDN));
  SetCardProxy();
  InitProviderDB();
  capability_->GetIdentifiers();
  EXPECT_EQ(kIMEI, cellular_->imei());
  EXPECT_EQ(kIMSI, cellular_->imsi());
  EXPECT_EQ(kTestCarrier, capability_->spn());
  EXPECT_EQ(kMSISDN, cellular_->mdn());
  EXPECT_EQ(kTestCarrier, cellular_->home_provider().GetName());
  EXPECT_EQ("us", cellular_->home_provider().GetCountry());
  EXPECT_EQ("310038", cellular_->home_provider().GetCode());
  capability_->GetIdentifiers();
  EXPECT_EQ(kIMEI, cellular_->imei());
  EXPECT_EQ(kIMSI, cellular_->imsi());
  EXPECT_EQ(kTestCarrier, capability_->spn());
  EXPECT_EQ(kMSISDN, cellular_->mdn());
}

TEST_F(CellularCapabilityGSMTest, GetSignalQuality) {
  const int kStrength = 80;
  EXPECT_CALL(*network_proxy_, GetSignalQuality()).WillOnce(Return(kStrength));
  SetNetworkProxy();
  SetService();
  EXPECT_EQ(0, cellular_->service()->strength());
  capability_->GetSignalQuality();
  EXPECT_EQ(kStrength, cellular_->service()->strength());
}

TEST_F(CellularCapabilityGSMTest, RegisterOnNetwork) {
  Error error;
  EXPECT_CALL(*network_proxy_, Register(kTestCarrier));
  capability_->RegisterOnNetwork(kTestCarrier, &error);
  EXPECT_TRUE(error.IsSuccess());
  SetNetworkProxy();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(kTestCarrier, capability_->selected_network_);
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

TEST_F(CellularCapabilityGSMTest, RequirePIN) {
  Error error;
  EXPECT_CALL(*card_proxy_, EnablePIN(kPIN, true));
  capability_->RequirePIN(kPIN, true, &error);
  EXPECT_TRUE(error.IsSuccess());
  SetCardProxy();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, EnterPIN) {
  Error error;
  EXPECT_CALL(*card_proxy_, SendPIN(kPIN));
  MockReturner returner;
  EXPECT_CALL(returner, Return());
  EXPECT_CALL(returner, ReturnError(_)).Times(0);
  capability_->EnterPIN(kPIN, &returner);
  SetCardProxy();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, UnblockPIN) {
  Error error;
  EXPECT_CALL(*card_proxy_, SendPUK(kPUK, kPIN));
  capability_->UnblockPIN(kPUK, kPIN, &error);
  EXPECT_TRUE(error.IsSuccess());
  SetCardProxy();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, ChangePIN) {
  static const char kOldPIN[] = "1111";
  Error error;
  EXPECT_CALL(*card_proxy_, ChangePIN(kOldPIN, kPIN));
  capability_->ChangePIN(kOldPIN, kPIN, &error);
  EXPECT_TRUE(error.IsSuccess());
  SetCardProxy();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, Scan) {
  static const char kID0[] = "123";
  static const char kID1[] = "456";
  Error error;
  capability_->Scan(&error);
  EXPECT_TRUE(error.IsSuccess());
  ModemGSMNetworkProxyInterface::ScanResults results;
  results.push_back(ModemGSMNetworkProxyInterface::ScanResult());
  results[0][CellularCapabilityGSM::kNetworkPropertyID] = kID0;
  results.push_back(ModemGSMNetworkProxyInterface::ScanResult());
  results[1][CellularCapabilityGSM::kNetworkPropertyID] = kID1;
  EXPECT_CALL(*network_proxy_, Scan()).WillOnce(Return(results));
  SetNetworkProxy();
  capability_->found_networks_.resize(3, Stringmap());
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(2, capability_->found_networks_.size());
  EXPECT_EQ(kID0,
            capability_->found_networks_[0][flimflam::kNetworkIdProperty]);
  EXPECT_EQ(kID1,
            capability_->found_networks_[1][flimflam::kNetworkIdProperty]);
}

TEST_F(CellularCapabilityGSMTest, ParseScanResult) {
  static const char kID[] = "123";
  static const char kLongName[] = "long name";
  static const char kShortName[] = "short name";
  ModemGSMNetworkProxyInterface::ScanResult result;
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
  ModemGSMNetworkProxyInterface::ScanResult result;
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
  cellular_->set_imsi("310240123456789");
  props[Cellular::kPropertyIMSI].writer().append_string("");
  capability_->UpdateStatus(props);
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
}

TEST_F(CellularCapabilityGSMTest, SetHomeProvider) {
  static const char kCountry[] = "us";
  static const char kCode[] = "310160";
  cellular_->set_imsi("310240123456789");

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

}  // namespace shill
