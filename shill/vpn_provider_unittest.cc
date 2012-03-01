// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_provider.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_vpn_driver.h"
#include "shill/vpn_service.h"

using std::string;
using testing::_;
using testing::Return;

namespace shill {

class VPNProviderTest : public testing::Test {
 public:
  VPNProviderTest()
      : manager_(&control_, NULL, &metrics_, NULL),
        provider_(&control_, NULL, &metrics_, &manager_) {}

  virtual ~VPNProviderTest() {}

 protected:
  NiceMockControl control_;
  MockMetrics metrics_;
  MockManager manager_;
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

TEST_F(VPNProviderTest, OnDeviceInfoAvailable) {
  const string kInterfaceName("tun0");
  const int kInterfaceIndex = 1;

  scoped_ptr<MockVPNDriver> bad_driver(new MockVPNDriver());
  EXPECT_CALL(*bad_driver.get(), ClaimInterface(_, _))
      .Times(2)
      .WillRepeatedly(Return(false));
  provider_.services_.push_back(
      new VPNService(&control_, NULL, &metrics_, NULL, bad_driver.release()));

  EXPECT_FALSE(provider_.OnDeviceInfoAvailable(kInterfaceName,
                                               kInterfaceIndex));

  scoped_ptr<MockVPNDriver> good_driver(new MockVPNDriver());
  EXPECT_CALL(*good_driver.get(), ClaimInterface(_, _))
      .WillOnce(Return(true));
  provider_.services_.push_back(
      new VPNService(&control_, NULL, &metrics_, NULL, good_driver.release()));

  scoped_ptr<MockVPNDriver> dup_driver(new MockVPNDriver());
  EXPECT_CALL(*dup_driver.get(), ClaimInterface(_, _))
      .Times(0);
  provider_.services_.push_back(
      new VPNService(&control_, NULL, &metrics_, NULL, dup_driver.release()));

  EXPECT_TRUE(provider_.OnDeviceInfoAvailable(kInterfaceName, kInterfaceIndex));
  provider_.services_.clear();
}

}  // namespace shill
