// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_modem_gsm_card_proxy.h"
#include "shill/mock_modem_gsm_network_proxy.h"
#include "shill/nice_mock_control.h"

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
        capability_(cellular_.get()) {}

  virtual ~CellularCapabilityGSMTest() {
    cellular_->service_ = NULL;
  }

 protected:
  static const char kTestCarrier[];
  static const char kPIN[];
  static const char kPUK[];
  static const char kIMEI[];
  static const char kIMSI[];
  static const char kMSISDN[];

  void SetCardProxy() {
    capability_.card_proxy_.reset(card_proxy_.release());
  }

  void SetAccessTechnology(uint32 technology) {
    cellular_->gsm_.access_technology = technology;
  }

  void SetRegistrationState(uint32 state) {
    cellular_->gsm_.registration_state = state;
  }

  void SetNetworkProxy() {
    cellular_->set_modem_gsm_network_proxy(network_proxy_.release());
  }

  void SetService() {
    cellular_->service_ = new CellularService(
        &control_, &dispatcher_, NULL, cellular_);
  }

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  CellularRefPtr cellular_;
  scoped_ptr<MockModemGSMCardProxy> card_proxy_;
  scoped_ptr<MockModemGSMNetworkProxy> network_proxy_;
  CellularCapabilityGSM capability_;
};

const char CellularCapabilityGSMTest::kTestCarrier[] = "The Cellular Carrier";
const char CellularCapabilityGSMTest::kPIN[] = "9876";
const char CellularCapabilityGSMTest::kPUK[] = "8765";
const char CellularCapabilityGSMTest::kIMEI[] = "987654321098765";
const char CellularCapabilityGSMTest::kIMSI[] = "123456789012345";
const char CellularCapabilityGSMTest::kMSISDN[] = "12345678901";

TEST_F(CellularCapabilityGSMTest, GetIdentifiers) {
  EXPECT_CALL(*card_proxy_, GetIMEI()).WillOnce(Return(kIMEI));
  EXPECT_CALL(*card_proxy_, GetIMSI()).WillOnce(Return(kIMSI));
  EXPECT_CALL(*card_proxy_, GetSPN()).WillOnce(Return(kTestCarrier));
  EXPECT_CALL(*card_proxy_, GetMSISDN()).WillOnce(Return(kMSISDN));
  SetCardProxy();
  capability_.GetIdentifiers();
  EXPECT_EQ(kIMEI, cellular_->imei());
  EXPECT_EQ(kIMSI, cellular_->imsi());
  EXPECT_EQ(kTestCarrier, cellular_->spn());
  EXPECT_EQ(kMSISDN, cellular_->mdn());
  capability_.GetIdentifiers();
  EXPECT_EQ(kIMEI, cellular_->imei());
  EXPECT_EQ(kIMSI, cellular_->imsi());
  EXPECT_EQ(kTestCarrier, cellular_->spn());
  EXPECT_EQ(kMSISDN, cellular_->mdn());
}

TEST_F(CellularCapabilityGSMTest, GetSignalQuality) {
  const int kStrength = 80;
  EXPECT_CALL(*network_proxy_, GetSignalQuality()).WillOnce(Return(kStrength));
  SetNetworkProxy();
  SetService();
  EXPECT_EQ(0, cellular_->service()->strength());
  capability_.GetSignalQuality();
  EXPECT_EQ(kStrength, cellular_->service()->strength());
}

TEST_F(CellularCapabilityGSMTest, RequirePIN) {
  Error error;
  EXPECT_CALL(*card_proxy_, EnablePIN(kPIN, true)).Times(1);
  capability_.RequirePIN(kPIN, true, &error);
  EXPECT_TRUE(error.IsSuccess());
  SetCardProxy();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, EnterPIN) {
  Error error;
  EXPECT_CALL(*card_proxy_, SendPIN(kPIN)).Times(1);
  capability_.EnterPIN(kPIN, &error);
  EXPECT_TRUE(error.IsSuccess());
  SetCardProxy();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, UnblockPIN) {
  Error error;
  EXPECT_CALL(*card_proxy_, SendPUK(kPUK, kPIN)).Times(1);
  capability_.UnblockPIN(kPUK, kPIN, &error);
  EXPECT_TRUE(error.IsSuccess());
  SetCardProxy();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, ChangePIN) {
  static const char kOldPIN[] = "1111";
  Error error;
  EXPECT_CALL(*card_proxy_, ChangePIN(kOldPIN, kPIN)).Times(1);
  capability_.ChangePIN(kOldPIN, kPIN, &error);
  EXPECT_TRUE(error.IsSuccess());
  SetCardProxy();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, GetNetworkTechnologyString) {
  EXPECT_EQ("", capability_.GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GSM);
  EXPECT_EQ("", capability_.GetNetworkTechnologyString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_HOME);
  EXPECT_EQ(flimflam::kNetworkTechnologyGsm,
            capability_.GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GSM_COMPACT);
  EXPECT_EQ(flimflam::kNetworkTechnologyGsm,
            capability_.GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GPRS);
  EXPECT_EQ(flimflam::kNetworkTechnologyGprs,
            capability_.GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_EDGE);
  EXPECT_EQ(flimflam::kNetworkTechnologyEdge,
            capability_.GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_UMTS);
  EXPECT_EQ(flimflam::kNetworkTechnologyUmts,
            capability_.GetNetworkTechnologyString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING);
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSDPA);
  EXPECT_EQ(flimflam::kNetworkTechnologyHspa,
            capability_.GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSUPA);
  EXPECT_EQ(flimflam::kNetworkTechnologyHspa,
            capability_.GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSPA);
  EXPECT_EQ(flimflam::kNetworkTechnologyHspa,
            capability_.GetNetworkTechnologyString());
  SetAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_HSPA_PLUS);
  EXPECT_EQ(flimflam::kNetworkTechnologyHspaPlus,
            capability_.GetNetworkTechnologyString());
}

TEST_F(CellularCapabilityGSMTest, GetRoamingStateString) {
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_.GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_HOME);
  EXPECT_EQ(flimflam::kRoamingStateHome,
            capability_.GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING);
  EXPECT_EQ(flimflam::kRoamingStateRoaming,
            capability_.GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_SEARCHING);
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_.GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_DENIED);
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_.GetRoamingStateString());
  SetRegistrationState(MM_MODEM_GSM_NETWORK_REG_STATUS_IDLE);
  EXPECT_EQ(flimflam::kRoamingStateUnknown,
            capability_.GetRoamingStateString());
}

}  // namespace shill
