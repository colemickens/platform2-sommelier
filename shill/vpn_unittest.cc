// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn.h"

#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/nice_mock_control.h"

namespace shill {

class VPNTest : public testing::Test {
 public:
  VPNTest()
      : manager_(&control_, &dispatcher_, &metrics_, &glib_),
        vpn_(new VPN(&control_,
                     &dispatcher_,
                     &metrics_,
                     &manager_,
                     kTestDeviceName,
                     kTestInterfaceIndex)) {}

  virtual ~VPNTest() {}

 protected:
  static const char kTestDeviceName[];
  static const int kTestInterfaceIndex;

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;

  VPNRefPtr vpn_;
};

const char VPNTest::kTestDeviceName[] = "tun0";
const int VPNTest::kTestInterfaceIndex = 5;

TEST_F(VPNTest, TechnologyIs) {
  EXPECT_TRUE(vpn_->TechnologyIs(Technology::kVPN));
  EXPECT_FALSE(vpn_->TechnologyIs(Technology::kEthernet));
}

}  // namespace shill
