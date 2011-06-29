// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/service.h"

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/dbus_adaptor.h"
#include "shill/ethernet_service.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_service.h"
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

class ServiceTest : public PropertyStoreTest {
 public:
  ServiceTest()
      : service_(new MockService(&control_interface_,
                                 &dispatcher_,
                                 "mock-service")) {
  }

  virtual ~ServiceTest() {}

 protected:
  ServiceRefPtr service_;
};

TEST_F(ServiceTest, Dispatch) {
  ::DBus::Error e1, e2, e3, e4;
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_.get(),
                                          flimflam::kSaveCredentialsProperty,
                                          PropertyStoreTest::kBoolV,
                                          e1));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_.get(),
                                          flimflam::kPriorityProperty,
                                          PropertyStoreTest::kInt32V,
                                          e2));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_.get(),
                                          flimflam::kEAPEAPProperty,
                                          PropertyStoreTest::kStringV,
                                          e3));

  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service_.get(),
                                           flimflam::kFavoriteProperty,
                                           PropertyStoreTest::kBoolV,
                                           e4));
  EXPECT_EQ(invalid_args_, e4.name());
}

TEST_P(ServiceTest, TestProperty) {
  // Ensure that an attempt to write unknown properties returns InvalidProperty.
  ::DBus::Error error;
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service_.get(),
                                           "",
                                           GetParam(),
                                           error));
  EXPECT_EQ(invalid_prop_, error.name());
}

INSTANTIATE_TEST_CASE_P(
    ServiceTestInstance,
    ServiceTest,
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
