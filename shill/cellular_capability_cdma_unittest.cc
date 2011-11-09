// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_cdma.h"

#include <gtest/gtest.h>

#include "shill/cellular.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_modem_cdma_proxy.h"
#include "shill/nice_mock_control.h"

using testing::Return;

namespace shill {

class CellularCapabilityCDMATest : public testing::Test {
 public:
  CellularCapabilityCDMATest()
      : cellular_(new Cellular(&control_,
                               &dispatcher_,
                               NULL,
                               "",
                               "",
                               0,
                               Cellular::kTypeCDMA,
                               "",
                               "",
                               NULL)),
        proxy_(new MockModemCDMAProxy()),
        capability_(cellular_.get()) {}

 protected:
  static const char kMEID[];

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  CellularRefPtr cellular_;
  scoped_ptr<MockModemCDMAProxy> proxy_;
  CellularCapabilityCDMA capability_;
};

const char CellularCapabilityCDMATest::kMEID[] = "D1234567EF8901";

TEST_F(CellularCapabilityCDMATest, GetIdentifiers) {
  EXPECT_CALL(*proxy_, MEID()).WillOnce(Return(kMEID));
  cellular_->set_modem_cdma_proxy(proxy_.release());
  capability_.GetIdentifiers();
  EXPECT_EQ(kMEID, cellular_->meid());
  capability_.GetIdentifiers();
  EXPECT_EQ(kMEID, cellular_->meid());
}

}  // namespace shill
