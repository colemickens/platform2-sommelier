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
#include "shill/mock_adaptors.h"
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
  static const char kMockServiceName[];
  static const char kMockDeviceRpcId[];

  ServiceTest()
      : service_(new MockService(&control_interface_,
                                 &dispatcher_,
                                 kMockServiceName)) {
  }

  virtual ~ServiceTest() {}

 protected:
  scoped_refptr<MockService> service_;
};

const char ServiceTest::kMockServiceName[] = "mock-service";

const char ServiceTest::kMockDeviceRpcId[] = "mock-device-rpc";

TEST_F(ServiceTest, GetProperties) {
  EXPECT_CALL(*service_.get(), CalculateState()).WillRepeatedly(Return(""));
  EXPECT_CALL(*service_.get(), GetDeviceRpcId())
      .WillRepeatedly(Return(ServiceTest::kMockDeviceRpcId));
  map<string, ::DBus::Variant> props;
  Error error(Error::kInvalidProperty, "");
  {
    ::DBus::Error dbus_error;
    string expected("portal_list");
    service_->store()->SetStringProperty(flimflam::kCheckPortalProperty,
                                         expected,
                                         &error);
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kCheckPortalProperty) == props.end());
    EXPECT_EQ(props[flimflam::kCheckPortalProperty].reader().get_string(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    bool expected = true;
    service_->store()->SetBoolProperty(flimflam::kAutoConnectProperty,
                                       expected,
                                       &error);
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kAutoConnectProperty) == props.end());
    EXPECT_EQ(props[flimflam::kAutoConnectProperty].reader().get_bool(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kConnectableProperty) == props.end());
    EXPECT_EQ(props[flimflam::kConnectableProperty].reader().get_bool(), false);
  }
  {
    ::DBus::Error dbus_error;
    int32 expected = 127;
    service_->store()->SetInt32Property(flimflam::kPriorityProperty,
                                        expected,
                                        &error);
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kPriorityProperty) == props.end());
    EXPECT_EQ(props[flimflam::kPriorityProperty].reader().get_int32(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(service_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kDeviceProperty) == props.end());
    EXPECT_EQ(props[flimflam::kDeviceProperty].reader().get_string(),
              string(ServiceTest::kMockDeviceRpcId));
  }
}

TEST_F(ServiceTest, Dispatch) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_->store(),
                                            flimflam::kSaveCredentialsProperty,
                                            PropertyStoreTest::kBoolV,
                                            &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_->store(),
                                            flimflam::kPriorityProperty,
                                            PropertyStoreTest::kInt32V,
                                            &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_->store(),
                                            flimflam::kEAPEAPProperty,
                                            PropertyStoreTest::kStringV,
                                            &error));
  }
  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(service_->store(),
                                             flimflam::kFavoriteProperty,
                                             PropertyStoreTest::kBoolV,
                                             &error));
    EXPECT_EQ(invalid_args_, error.name());
  }
}

TEST_P(ServiceTest, TestProperty) {
  // Ensure that an attempt to write unknown properties returns InvalidProperty.
  ::DBus::Error error;
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service_->store(),
                                           "",
                                           GetParam(),
                                           &error));
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
