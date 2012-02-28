// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_provider.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_metrics.h"
#include "shill/vpn_service.h"

namespace shill {

class VPNProviderTest : public testing::Test {
 public:
  VPNProviderTest()
      : provider_(&control_, NULL, &metrics_, NULL) {}

  virtual ~VPNProviderTest() {}

 protected:
  NiceMockControl control_;
  MockMetrics metrics_;
  VPNProvider provider_;
};

TEST_F(VPNProviderTest, GetServiceVPNNoType) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeVPN);
  VPNServiceRefPtr service = provider_.GetService(args, &e);
  EXPECT_EQ(Error::kNotSupported, e.type());
  EXPECT_FALSE(service);
}

TEST_F(VPNProviderTest, GetServiceVPNUnsupportedType) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeVPN);
  args.SetString(flimflam::kProviderTypeProperty, "unknown-vpn-type");
  VPNServiceRefPtr service = provider_.GetService(args, &e);
  EXPECT_EQ(Error::kNotSupported, e.type());
  EXPECT_FALSE(service);
}

TEST_F(VPNProviderTest, GetServiceVPN) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeVPN);
  args.SetString(flimflam::kProviderTypeProperty, flimflam::kProviderOpenVpn);
  VPNServiceRefPtr service = provider_.GetService(args, &e);
  EXPECT_TRUE(e.IsSuccess());
  EXPECT_TRUE(service);
}

}  // namespace shill
