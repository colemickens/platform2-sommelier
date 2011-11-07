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
        gsm_card_proxy_(new MockModemGSMCardProxy()),
        capability_(cellular_.get()) {}

 protected:
  static const char kPIN[];
  static const char kPUK[];

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  CellularRefPtr cellular_;
  scoped_ptr<MockModemGSMCardProxy> gsm_card_proxy_;
  CellularCapabilityGSM capability_;
};

const char CellularCapabilityGSMTest::kPIN[] = "9876";
const char CellularCapabilityGSMTest::kPUK[] = "8765";

TEST_F(CellularCapabilityGSMTest, RequirePIN) {
  Error error;
  EXPECT_CALL(*gsm_card_proxy_, EnablePIN(kPIN, true)).Times(1);
  capability_.RequirePIN(kPIN, true, &error);
  EXPECT_TRUE(error.IsSuccess());
  cellular_->set_modem_gsm_card_proxy(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, EnterPIN) {
  Error error;
  EXPECT_CALL(*gsm_card_proxy_, SendPIN(kPIN)).Times(1);
  capability_.EnterPIN(kPIN, &error);
  EXPECT_TRUE(error.IsSuccess());
  cellular_->set_modem_gsm_card_proxy(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, UnblockPIN) {
  Error error;
  EXPECT_CALL(*gsm_card_proxy_, SendPUK(kPUK, kPIN)).Times(1);
  capability_.UnblockPIN(kPUK, kPIN, &error);
  EXPECT_TRUE(error.IsSuccess());
  cellular_->set_modem_gsm_card_proxy(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularCapabilityGSMTest, ChangePIN) {
  static const char kOldPIN[] = "1111";
  Error error;
  EXPECT_CALL(*gsm_card_proxy_, ChangePIN(kOldPIN, kPIN)).Times(1);
  capability_.ChangePIN(kOldPIN, kPIN, &error);
  EXPECT_TRUE(error.IsSuccess());
  cellular_->set_modem_gsm_card_proxy(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

}  // namespace shill
