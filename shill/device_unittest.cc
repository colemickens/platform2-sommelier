// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device.h"

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/dbus_adaptor.h"
#include "shill/dhcp_provider.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_glib.h"
#include "shill/property_store_unittest.h"
#include "shill/shill_event.h"

using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Test;
using ::testing::Values;

namespace shill {

namespace {
const char kDeviceName[] = "testdevice";
}  // namespace {}

class DeviceTest : public PropertyStoreTest {
 public:
  DeviceTest()
      : device_(new Device(&control_interface_, NULL, NULL, kDeviceName, 0)) {
    DHCPProvider::GetInstance()->glib_ = &glib_;
  }
  virtual ~DeviceTest() {}

 protected:
  MockGLib glib_;
  MockControl control_interface_;
  DeviceRefPtr device_;
};

TEST_F(DeviceTest, Contains) {
  EXPECT_TRUE(device_->Contains(flimflam::kNameProperty));
  EXPECT_FALSE(device_->Contains(""));
}

TEST_F(DeviceTest, Dispatch) {
  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_.get(),
                                          flimflam::kPoweredProperty,
                                          PropertyStoreTest::kBoolV,
                                          error));

  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_.get(),
                                           flimflam::kAddressProperty,
                                           PropertyStoreTest::kStringV,
                                           error));
  EXPECT_EQ(invalid_args_, error.name());
}

TEST_P(DeviceTest, TestProperty) {
  // Ensure that an attempt to write unknown properties returns InvalidProperty.
  ::DBus::Error error;
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", GetParam(), error));
  EXPECT_EQ(invalid_prop_, error.name());
}

INSTANTIATE_TEST_CASE_P(
    DeviceTestInstance,
    DeviceTest,
    Values(PropertyStoreTest::kBoolV,
           PropertyStoreTest::kByteV,
           PropertyStoreTest::kStringV,
           PropertyStoreTest::kInt16V,
           PropertyStoreTest::kInt32V,
           PropertyStoreTest::kUint16V,
           PropertyStoreTest::kUint32V,
           PropertyStoreTest::kStringsV,
           PropertyStoreTest::kStringmapV,
           PropertyStoreTest::kStringmapsV));

TEST_F(DeviceTest, TechnologyIs) {
  EXPECT_FALSE(device_->TechnologyIs(Device::kEthernet));
}

TEST_F(DeviceTest, DestroyIPConfig) {
  ASSERT_FALSE(device_->ipconfig_.get());
  device_->ipconfig_ = new IPConfig(kDeviceName);
  device_->DestroyIPConfig();
  ASSERT_FALSE(device_->ipconfig_.get());
}

TEST_F(DeviceTest, DestroyIPConfigNULL) {
  ASSERT_FALSE(device_->ipconfig_.get());
  device_->DestroyIPConfig();
  ASSERT_FALSE(device_->ipconfig_.get());
}

TEST_F(DeviceTest, AcquireDHCPConfig) {
  device_->ipconfig_ = new IPConfig("randomname");
  EXPECT_CALL(glib_, SpawnAsync(_, _, _, _, _, _, _, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(device_->AcquireDHCPConfig());
  ASSERT_TRUE(device_->ipconfig_.get());
  EXPECT_EQ(kDeviceName, device_->ipconfig_->device_name());
  EXPECT_TRUE(device_->ipconfig_->update_callback_.get());
}

}  // namespace shill
