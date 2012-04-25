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
#include "shill/mock_profile.h"
#include "shill/mock_store.h"
#include "shill/mock_vpn_driver.h"
#include "shill/mock_vpn_service.h"

using std::string;
using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;

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

MATCHER_P(ServiceWithStorageId, storage_id, "") {
  return arg->GetStorageIdentifier() == storage_id;
}

TEST_F(VPNProviderTest, CreateServicesFromProfile) {
  scoped_refptr<MockProfile> profile(
      new NiceMock<MockProfile>(&control_, &manager_, ""));
  NiceMock<MockStore> storage;
  EXPECT_CALL(*profile, GetConstStorage())
      .WillRepeatedly(Return(&storage));
  EXPECT_CALL(storage, GetString(_, _, _))
      .WillRepeatedly(Return(false));

  std::set<string> groups;
  const string kNonVPNIdentifier("foo_123_456");
  groups.insert(kNonVPNIdentifier);
  const string kVPNIdentifier0("vpn_123_456");
  const string kVPNIdentifier1("vpn_789_012");
  groups.insert(kVPNIdentifier0);
  groups.insert(kVPNIdentifier1);
  EXPECT_CALL(storage, GetGroupsWithKey(flimflam::kProviderTypeProperty))
      .WillRepeatedly(Return(groups));

  EXPECT_CALL(*profile, ConfigureService(ServiceWithStorageId(kVPNIdentifier1)))
      .WillOnce(Return(true));

  const string kName("name");
  EXPECT_CALL(storage, GetString(_, flimflam::kNameProperty, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(kName), Return(true)));

  const string kProviderTypeUnknown("unknown");
  EXPECT_CALL(storage, GetString(kVPNIdentifier0,
                                 flimflam::kProviderTypeProperty,
                                 _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(kProviderTypeUnknown),
                            Return(true)));

  const string kProviderTypeOpenVPN(flimflam::kProviderOpenVpn);
  EXPECT_CALL(storage, GetString(kVPNIdentifier1,
                                 flimflam::kProviderTypeProperty,
                                 _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(kProviderTypeOpenVPN),
                            Return(true)));

  EXPECT_CALL(manager_, device_info())
      .WillRepeatedly(Return(reinterpret_cast<DeviceInfo *>(NULL)));

  EXPECT_CALL(manager_, RegisterService(ServiceWithStorageId(kVPNIdentifier1)))
      .Times(1);

  provider_.CreateServicesFromProfile(profile);

  // Calling this again should not create any more services (checked by the
  // Times(1) above).
  provider_.CreateServicesFromProfile(profile);
}

TEST_F(VPNProviderTest, CreateService) {
  static const char kName[] = "test-vpn-service";
  static const char kStorageID[] = "test_vpn_storage_id";
  static const char *kTypes[] = {
    flimflam::kProviderOpenVpn,
    flimflam::kProviderL2tpIpsec,
  };
  const size_t kTypesCount = arraysize(kTypes);
  EXPECT_CALL(manager_, device_info())
      .Times(kTypesCount)
      .WillRepeatedly(Return(&device_info_));
  EXPECT_CALL(manager_, RegisterService(_)).Times(kTypesCount);
  for (size_t i = 0; i < kTypesCount; i++) {
    Error error;
    VPNServiceRefPtr service =
        provider_.CreateService(kTypes[i], kName, kStorageID, &error);
    ASSERT_TRUE(service) << kTypes[i];
    ASSERT_TRUE(service->driver()) << kTypes[i];
    EXPECT_EQ(kTypes[i], service->driver()->GetProviderType());
    EXPECT_EQ(kName, service->friendly_name()) << kTypes[i];
    EXPECT_EQ(kStorageID, service->GetStorageIdentifier()) << kTypes[i];
    EXPECT_TRUE(error.IsSuccess()) << kTypes[i];
  }
  Error error;
  VPNServiceRefPtr unknown_service =
      provider_.CreateService("unknown-vpn-type", kName, kStorageID, &error);
  EXPECT_FALSE(unknown_service);
  EXPECT_EQ(Error::kNotSupported, error.type());
}

}  // namespace shill
