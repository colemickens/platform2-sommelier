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
using std::vector;

class ManagerTest : public PropertyStoreTest {
 public:
  ManagerTest() : factory_(this) {}
  virtual ~ManagerTest() {}

  bool IsDeviceRegistered(Device *device, Device::Technology tech) {
    vector<DeviceRefPtr> devices;
    manager_.FilterByTechnology(tech, &devices);
    return (devices.size() == 1 && devices[0].get() == device);
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

  EXPECT_TRUE(IsDeviceRegistered(mock_device.get(), Device::kEthernet));
  EXPECT_TRUE(IsDeviceRegistered(mock_device2.get(), Device::kWifi));
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

  ASSERT_TRUE(IsDeviceRegistered(mock_device.get(), Device::kEthernet));
  ASSERT_TRUE(IsDeviceRegistered(mock_device2.get(), Device::kWifi));

  manager_.DeregisterDevice(mock_device.get());
  EXPECT_FALSE(IsDeviceRegistered(mock_device.get(), Device::kEthernet));

  manager_.DeregisterDevice(mock_device2.get());
  EXPECT_FALSE(IsDeviceRegistered(mock_device2.get(), Device::kWifi));
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
                                device.get(),
                                kService1));

  scoped_refptr<MockService> mock_service2(
      new NiceMock<MockService>(&control_interface_,
                                &dispatcher_,
                                device.get(),
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
                                          bool_v_,
                                          error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&manager_,
                                          flimflam::kActiveProfileProperty,
                                          string_v_,
                                          error));

  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", bool_v_, error));
  EXPECT_EQ(invalid_prop_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", strings_v_, error));
  EXPECT_EQ(invalid_prop_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", int16_v_, error));
  EXPECT_EQ(invalid_prop_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", int32_v_, error));
  EXPECT_EQ(invalid_prop_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", uint16_v_, error));
  EXPECT_EQ(invalid_prop_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", uint32_v_, error));
  EXPECT_EQ(invalid_prop_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", stringmap_v_, error));
  EXPECT_EQ(invalid_prop_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", byte_v_, error));
  EXPECT_EQ(invalid_prop_, error.name());

  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_,
                                           flimflam::kActiveProfileProperty,
                                           bool_v_,
                                           error));
  EXPECT_EQ(invalid_args_, error.name());
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_,
                                           flimflam::kOfflineModeProperty,
                                           string_v_,
                                           error));
  EXPECT_EQ(invalid_args_, error.name());
}

}  // namespace shill
