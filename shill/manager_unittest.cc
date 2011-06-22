// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager.h"

#include <glib.h>

#include <base/callback_old.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_service.h"
#include "shill/property_store_unittest.h"
#include "shill/shill_event.h"

using std::map;
using std::string;
using std::vector;


namespace shill {
using ::testing::Test;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Values;

class ManagerTest : public PropertyStoreTest {
 public:
  ManagerTest() : factory_(this) {}
  virtual ~ManagerTest() {}

  bool IsDeviceRegistered(const DeviceRefPtr &device, Device::Technology tech) {
    vector<DeviceRefPtr> devices;
    manager_.FilterByTechnology(tech, &devices);
    return (devices.size() == 1 && devices[0].get() == device.get());
  }

 protected:
  ScopedRunnableMethodFactory<ManagerTest> factory_;
};

TEST_F(ManagerTest, Contains) {
  EXPECT_TRUE(manager_.Contains(flimflam::kStateProperty));
  EXPECT_FALSE(manager_.Contains(""));
}

TEST_F(ManagerTest, DeviceRegistration) {
  scoped_refptr<MockDevice> mock_device(
      new NiceMock<MockDevice>(&control_interface_,
                               &dispatcher_,
                               &manager_,
                               "null0",
                               -1));
  ON_CALL(*mock_device.get(), TechnologyIs(Device::kEthernet))
      .WillByDefault(Return(true));

  scoped_refptr<MockDevice> mock_device2(
      new NiceMock<MockDevice>(&control_interface_,
                               &dispatcher_,
                               &manager_,
                               "null1",
                               -1));
  ON_CALL(*mock_device2.get(), TechnologyIs(Device::kWifi))
      .WillByDefault(Return(true));

  manager_.RegisterDevice(mock_device.get());
  manager_.RegisterDevice(mock_device2.get());

  EXPECT_TRUE(IsDeviceRegistered(mock_device, Device::kEthernet));
  EXPECT_TRUE(IsDeviceRegistered(mock_device2, Device::kWifi));
}

TEST_F(ManagerTest, DeviceDeregistration) {
  scoped_refptr<MockDevice> mock_device(
      new NiceMock<MockDevice>(&control_interface_,
                               &dispatcher_,
                               &manager_,
                               "null2",
                               -1));
  ON_CALL(*mock_device.get(), TechnologyIs(Device::kEthernet))
      .WillByDefault(Return(true));

  scoped_refptr<MockDevice> mock_device2(
      new NiceMock<MockDevice>(&control_interface_,
                               &dispatcher_,
                               &manager_,
                               "null2",
                               -1));
  ON_CALL(*mock_device2.get(), TechnologyIs(Device::kWifi))
      .WillByDefault(Return(true));

  manager_.RegisterDevice(mock_device.get());
  manager_.RegisterDevice(mock_device2.get());

  ASSERT_TRUE(IsDeviceRegistered(mock_device, Device::kEthernet));
  ASSERT_TRUE(IsDeviceRegistered(mock_device2, Device::kWifi));

  manager_.DeregisterDevice(mock_device.get());
  EXPECT_FALSE(IsDeviceRegistered(mock_device, Device::kEthernet));

  manager_.DeregisterDevice(mock_device2.get());
  EXPECT_FALSE(IsDeviceRegistered(mock_device2, Device::kWifi));
}

TEST_F(ManagerTest, ServiceRegistration) {
  scoped_refptr<MockDevice> device(new MockDevice(&control_interface_,
                                                  &dispatcher_,
                                                  &manager_,
                                                  "null3",
                                                  -1));
  const char kService1[] = "service1";
  const char kService2[] = "wifi_service2";
  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(&control_interface_,
                                &dispatcher_,
                                kService1));

  scoped_refptr<MockService> mock_service2(
      new NiceMock<MockService>(&control_interface_,
                                &dispatcher_,
                                kService2));

  manager_.RegisterService(mock_service);
  manager_.RegisterService(mock_service2);

  EXPECT_TRUE(manager_.FindService(kService1).get() != NULL);
  EXPECT_TRUE(manager_.FindService(kService2).get() != NULL);
}

TEST_F(ManagerTest, Dispatch) {
  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&manager_,
                                          flimflam::kOfflineModeProperty,
                                          PropertyStoreTest::kBoolV,
                                          error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&manager_,
                                          flimflam::kCountryProperty,
                                          PropertyStoreTest::kStringV,
                                          error));

  // Attempt to write with value of wrong type should return InvalidArgs.
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_,
                                           flimflam::kCountryProperty,
                                           PropertyStoreTest::kBoolV,
                                           error));
  EXPECT_EQ(invalid_args_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_,
                                           flimflam::kOfflineModeProperty,
                                           PropertyStoreTest::kStringV,
                                           error));
  EXPECT_EQ(invalid_args_, error.name());

  // Attempt to write R/O property should return InvalidArgs.
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(
      &manager_,
      flimflam::kEnabledTechnologiesProperty,
      PropertyStoreTest::kStringsV,
      error));
  EXPECT_EQ(invalid_args_, error.name());
}

TEST_P(ManagerTest, TestProperty) {
  // Ensure that an attempt to write unknown properties returns InvalidProperty.
  ::DBus::Error error;
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", GetParam(), error));
  EXPECT_EQ(invalid_prop_, error.name());
}

INSTANTIATE_TEST_CASE_P(
    ManagerTestInstance,
    ManagerTest,
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
