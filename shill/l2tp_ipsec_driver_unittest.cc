// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/l2tp_ipsec_driver.h"

#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/nice_mock_control.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_vpn_service.h"

namespace shill {

class L2TPIPSecDriverTest : public testing::Test {
 public:
  L2TPIPSecDriverTest()
      : manager_(&control_, &dispatcher_, &metrics_, &glib_),
        driver_(new L2TPIPSecDriver()),
        service_(new MockVPNService(&control_, &dispatcher_, &metrics_,
                                    &manager_, driver_)) {}

  virtual ~L2TPIPSecDriverTest() {}

 protected:
  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;
  L2TPIPSecDriver *driver_;  // Owned by |service_|.
  scoped_refptr<MockVPNService> service_;
};

TEST_F(L2TPIPSecDriverTest, GetProviderType) {
  EXPECT_EQ(flimflam::kProviderL2tpIpsec, driver_->GetProviderType());
}

}  // namespace shill
