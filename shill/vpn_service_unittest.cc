// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_service.h"

#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_metrics.h"
#include "shill/mock_vpn_driver.h"

namespace shill {

class VPNServiceTest : public testing::Test {
 public:
  VPNServiceTest()
      : driver_(new MockVPNDriver()),
        service_(new VPNService(&control_, NULL, &metrics_, NULL,
                                driver_)) {}

  virtual ~VPNServiceTest() {}

 protected:
  MockVPNDriver *driver_;  // Owned by |service_|.
  NiceMockControl control_;
  MockMetrics metrics_;
  VPNServiceRefPtr service_;
};

TEST_F(VPNServiceTest, Connect) {
  Error error;
  service_->Connect(&error);
  EXPECT_EQ(Error::kNotSupported, error.type());
}

TEST_F(VPNServiceTest, GetStorageIdentifier) {
  EXPECT_EQ("", service_->GetStorageIdentifier());
}

TEST_F(VPNServiceTest, GetDeviceRpcId) {
  Error error;
  EXPECT_EQ("/", service_->GetDeviceRpcId(&error));
  EXPECT_EQ(Error::kNotSupported, error.type());
}

}  // namespace shill
