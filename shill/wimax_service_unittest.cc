// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_service.h"

#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_wimax.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace shill {

namespace {

const char kTestLinkName[] = "wm0";
const int kTestInterfaceIndex = 5;
const char kTestPath[] = "/org/chromium/WiMaxManager/Device/7";

}  // namespace

class WiMaxServiceTest : public testing::Test {
 public:
  WiMaxServiceTest()
      : manager_(&control_, NULL, NULL, NULL),
        wimax_(new MockWiMax(&control_, NULL, &metrics_, &manager_,
                             kTestLinkName, kTestInterfaceIndex, kTestPath)),
        service_(new WiMaxService(&control_, NULL, &metrics_, &manager_,
                                  wimax_)) {}

  virtual ~WiMaxServiceTest() {}

 protected:
  NiceMockControl control_;
  MockManager manager_;
  MockMetrics metrics_;
  scoped_refptr<MockWiMax> wimax_;
  WiMaxServiceRefPtr service_;
};

TEST_F(WiMaxServiceTest, TechnologyIs) {
  EXPECT_TRUE(service_->TechnologyIs(Technology::kWiMax));
  EXPECT_FALSE(service_->TechnologyIs(Technology::kEthernet));
}

}  // namespace shill
