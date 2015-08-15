// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/subscription_state_out_of_credits_detector.h"

#include <memory>

#include <gtest/gtest.h>
#include "ModemManager/ModemManager.h"

#include "shill/cellular/mock_cellular.h"
#include "shill/cellular/mock_cellular_service.h"
#include "shill/cellular/mock_modem_info.h"
#include "shill/mock_connection.h"
#include "shill/mock_connection_health_checker.h"
#include "shill/mock_device_info.h"
#include "shill/mock_manager.h"
#include "shill/mock_traffic_monitor.h"
#include "shill/test_event_dispatcher.h"

using testing::Mock;
using testing::NiceMock;

namespace shill {

class SubscriptionStateOutOfCreditsDetectorTest : public testing::Test {
 public:
  SubscriptionStateOutOfCreditsDetectorTest()
      : modem_info_(nullptr, &dispatcher_, &metrics_, &manager_, nullptr),
        device_info_(modem_info_.control_interface(), modem_info_.dispatcher(),
                     modem_info_.metrics(), modem_info_.manager()),
        manager_(modem_info_.control_interface(), modem_info_.dispatcher(),
                 modem_info_.metrics(), modem_info_.glib()),
        metrics_(modem_info_.dispatcher()),
        cellular_(new NiceMock<MockCellular>(&modem_info_,
                                             "usb0",
                                             kAddress,
                                             3,
                                             Cellular::kTypeCDMA,
                                             "",
                                             "")),
        service_(new NiceMock<MockCellularService>(&modem_info_, cellular_)),
        connection_(new NiceMock<MockConnection>(&device_info_)),
        out_of_credits_detector_(
            new SubscriptionStateOutOfCreditsDetector(
                modem_info_.dispatcher(), modem_info_.manager(),
                modem_info_.metrics(), service_.get())) {}

  virtual void SetUp() {
    service_->connection_ = connection_;
    cellular_->service_ = service_;
    service_->SetRoamingState(kRoamingStateHome);
  }

  virtual void TearDown() {
    cellular_->service_ = nullptr;  // Break circular reference.
  }

 protected:
  static const char kAddress[];

  EventDispatcherForTest dispatcher_;
  MockModemInfo modem_info_;
  NiceMock<MockDeviceInfo> device_info_;
  NiceMock<MockManager> manager_;
  NiceMock<MockMetrics> metrics_;
  scoped_refptr<NiceMock<MockCellular>> cellular_;
  scoped_refptr<NiceMock<MockCellularService>> service_;
  scoped_refptr<NiceMock<MockConnection>> connection_;
  std::unique_ptr<SubscriptionStateOutOfCreditsDetector>
      out_of_credits_detector_;
};

const char
    SubscriptionStateOutOfCreditsDetectorTest::kAddress[] = "000102030405";

TEST_F(SubscriptionStateOutOfCreditsDetectorTest, OutOfCreditsDetection) {
  out_of_credits_detector_->NotifySubscriptionStateChanged(
      MM_MODEM_3GPP_SUBSCRIPTION_STATE_OUT_OF_DATA);
  EXPECT_TRUE(out_of_credits_detector_->out_of_credits());
  out_of_credits_detector_->NotifySubscriptionStateChanged(
      MM_MODEM_3GPP_SUBSCRIPTION_STATE_PROVISIONED);
  EXPECT_FALSE(out_of_credits_detector_->out_of_credits());
}

}  // namespace shill
