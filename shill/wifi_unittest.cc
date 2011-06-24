// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi.h"

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/dbus_adaptor.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/property_store_unittest.h"

using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Test;
using ::testing::Values;

namespace shill {

class WiFiTest : public PropertyStoreTest {
 public:
  WiFiTest()
      : device_(new WiFi(&control_interface_, NULL, NULL, "wifi", 0)) {
  }
  virtual ~WiFiTest() {}

 protected:
  DeviceRefPtr device_;
};

TEST_F(WiFiTest, Contains) {
  EXPECT_TRUE(device_->Contains(flimflam::kNameProperty));
  EXPECT_FALSE(device_->Contains(""));
}

TEST_F(WiFiTest, Dispatch) {
  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_.get(),
                                          flimflam::kBgscanMethodProperty,
                                          PropertyStoreTest::kStringV,
                                          error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(
      device_.get(),
      flimflam::kBgscanSignalThresholdProperty,
      PropertyStoreTest::kInt32V,
      error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_.get(),
                                          flimflam::kScanIntervalProperty,
                                          PropertyStoreTest::kUint16V,
                                          error));

  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_.get(),
                                           flimflam::kScanningProperty,
                                           PropertyStoreTest::kBoolV,
                                           error));
  EXPECT_EQ(invalid_args_, error.name());
}

TEST_P(WiFiTest, TestProperty) {
  // Ensure that an attempt to write unknown properties returns InvalidProperty.
  ::DBus::Error error;
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", GetParam(), error));
  EXPECT_EQ(invalid_prop_, error.name());
}

INSTANTIATE_TEST_CASE_P(
    WiFiTestInstance,
    WiFiTest,
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

}  // namespace shill
