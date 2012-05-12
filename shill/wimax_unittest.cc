// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax.h"

#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/nice_mock_control.h"

namespace shill {

namespace {

const char kTestLinkName[] = "wm0";
const int kTestInterfaceIndex = 5;

}  // namespace

class WiMaxTest : public testing::Test {
 public:
  WiMaxTest()
      : manager_(&control_, &dispatcher_, &metrics_, &glib_),
        wimax_(new WiMax(&control_, &dispatcher_, &metrics_, &manager_,
                         kTestLinkName, kTestInterfaceIndex)) {}

  virtual ~WiMaxTest() {}

 protected:
  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;

  WiMaxRefPtr wimax_;
};

TEST_F(WiMaxTest, TechnologyIs) {
  EXPECT_TRUE(wimax_->TechnologyIs(Technology::kWiMax));
  EXPECT_FALSE(wimax_->TechnologyIs(Technology::kEthernet));
}

}  // namespace shill
