// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_provider.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_device_info.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_vpn_driver.h"
#include "shill/mock_vpn_service.h"

using std::string;
using testing::_;
using testing::Return;

namespace shill {

class VPNProviderTest : public testing::Test {
 public:
  VPNProviderTest()
      : manager_(&control_, NULL, &metrics_, NULL),
        device_info_(&control_, NULL, &metrics_, &manager_),
        provider_(&control_, NULL, &metrics_, &manager_) {}

  virtual ~VPNProviderTest() {}

 protected:
  static const char kHost[];
  static const char kName[];

  NiceMockControl control_;
  MockMetrics metrics_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  VPNProvider provider_;
};

const char VPNProviderTest::kHost[] = "10.8.0.1";
const char VPNProviderTest::kName[] = "vpn-name";

TEST_F(VPNProviderTest, GetServiceNoType) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeVPN);
  VPNServiceRefPtr service = provider_.GetService(args, &e);
  EXPECT_EQ(Error::kNotSupported, e.type());
  EXPECT_FALSE(service);
}

TEST_F(VPNProviderTest, GetServiceUnsupportedType) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeVPN);
  args.SetString(flimflam::kProviderTypeProperty, "unknown-vpn-type");
  args.SetString(flimflam::kProviderHostProperty, kHost);
  args.SetString(flimflam::kProviderNameProperty, kName);
  VPNServiceRefPtr service = provider_.GetService(args, &e);
  EXPECT_EQ(Error::kNotSupported, e.type());
  EXPECT_FALSE(service);
}

TEST_F(VPNProviderTest, GetService) {
  KeyValueStore args;
  Error e;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeVPN);
  args.SetString(flimflam::kProviderTypeProperty, flimflam::kProviderOpenVpn);
  args.SetString(flimflam::kProviderHostProperty, kHost);
  args.SetString(flimflam::kProviderNameProperty, kName);
  EXPECT_CALL(manager_, device_info()).WillOnce(Return(&device_info_));
  EXPECT_CALL(manager_, RegisterService(_));
  VPNServiceRefPtr service0 = provider_.GetService(args, &e);
  EXPECT_TRUE(e.IsSuccess());
  ASSERT_TRUE(service0);
  EXPECT_EQ("vpn_10_8_0_1_vpn_name", service0->GetStorageIdentifier());
  EXPECT_EQ(kName, service0->friendly_name());

  // A second call should return the same service.
  VPNServiceRefPtr service1 = provider_.GetService(args, &e);
  EXPECT_TRUE(e.IsSuccess());
  ASSERT_EQ(service0.get(), service1.get());
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

TEST_F(VPNProviderTest, RemoveService) {
  scoped_refptr<MockVPNService> service0(
      new MockVPNService(&control_, NULL, &metrics_, NULL, NULL));
  scoped_refptr<MockVPNService> service1(
      new MockVPNService(&control_, NULL, &metrics_, NULL, NULL));
  scoped_refptr<MockVPNService> service2(
      new MockVPNService(&control_, NULL, &metrics_, NULL, NULL));

  provider_.services_.push_back(service0.get());
  provider_.services_.push_back(service1.get());
  provider_.services_.push_back(service2.get());

  ASSERT_EQ(3, provider_.services_.size());

  provider_.RemoveService(service1);

  EXPECT_EQ(2, provider_.services_.size());
  EXPECT_EQ(service0, provider_.services_[0]);
  EXPECT_EQ(service2, provider_.services_[1]);

  provider_.RemoveService(service2);

  EXPECT_EQ(1, provider_.services_.size());
  EXPECT_EQ(service0, provider_.services_[0]);

  provider_.RemoveService(service0);
  EXPECT_EQ(0, provider_.services_.size());
}

}  // namespace shill
