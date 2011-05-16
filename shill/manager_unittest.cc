// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <glib.h>

#include <base/callback_old.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/dbus_control.h"
#include "shill/manager.h"
#include "shill/mock_device.h"
#include "shill/mock_service.h"

namespace shill {
using ::testing::Test;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using std::vector;

class ManagerTest : public Test {
 public:
  ManagerTest()
      : manager_(&control_, &dispatcher_),
        factory_(this) {
  }

  bool IsDeviceRegistered(Device *device, Device::Technology tech) {
    vector<scoped_refptr<Device> > devices;
    manager_.FilterByTechnology(tech, &devices);
    return (devices.size() == 1 && devices[0].get() == device);
  }

protected:
  DBusControl control_;
  Manager manager_;
  EventDispatcher dispatcher_;
  ScopedRunnableMethodFactory<ManagerTest> factory_;
};

TEST_F(ManagerTest, DeviceRegistration) {
  scoped_refptr<MockDevice> mock_device(
      new NiceMock<MockDevice>(&control_, &dispatcher_, "null0", -1));
  ON_CALL(*mock_device.get(), TechnologyIs(Device::kEthernet))
      .WillByDefault(Return(true));

  scoped_refptr<MockDevice> mock_device2(
      new NiceMock<MockDevice>(&control_, &dispatcher_, "null1", -1));
  ON_CALL(*mock_device2.get(), TechnologyIs(Device::kWifi))
      .WillByDefault(Return(true));

  manager_.RegisterDevice(mock_device.get());
  manager_.RegisterDevice(mock_device2.get());

  EXPECT_TRUE(IsDeviceRegistered(mock_device.get(), Device::kEthernet));
  EXPECT_TRUE(IsDeviceRegistered(mock_device2.get(), Device::kWifi));
}

TEST_F(ManagerTest, DeviceDeregistration) {
  scoped_refptr<MockDevice> mock_device(
      new NiceMock<MockDevice>(&control_, &dispatcher_, "null2", -1));
  ON_CALL(*mock_device.get(), TechnologyIs(Device::kEthernet))
      .WillByDefault(Return(true));

  scoped_refptr<MockDevice> mock_device2(
      new NiceMock<MockDevice>(&control_, &dispatcher_, "null2", -1));
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
  scoped_refptr<MockDevice> device(new MockDevice(&control_, &dispatcher_,
                                                  "null3", -1));
  const char kService1[] = "service1";
  const char kService2[] = "wifi_service2";
  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(&control_, &dispatcher_, device.get()));
  mock_service->set_name(kService1);

  scoped_refptr<MockService> mock_service2(
      new NiceMock<MockService>(&control_, &dispatcher_, device.get()));
  mock_service2->set_name(kService2);

  manager_.RegisterService(mock_service.get());
  manager_.RegisterService(mock_service2.get());

  EXPECT_TRUE(manager_.FindService(kService1));
  EXPECT_TRUE(manager_.FindService(kService2));
}

}  // namespace shill
