// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <gtest/gtest.h>

#include "shill/cellular.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_modem_gsm_card_proxy.h"
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
        capability_(cellular_.get()) {}

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

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  CellularRefPtr cellular_;
  scoped_ptr<MockModemGSMCardProxy> card_proxy_;
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

}  // namespace shill
