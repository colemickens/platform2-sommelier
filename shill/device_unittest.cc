// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device.h"

#include <ctype.h>
#include <sys/socket.h>
#include <linux/if.h>  // Needs typedefs from sys/socket.h.

#include <map>
#include <string>
#include <vector>

#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/dbus_adaptor.h"
#include "shill/dhcp_provider.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_glib.h"
#include "shill/mock_ipconfig.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/mock_service.h"
#include "shill/mock_store.h"
#include "shill/property_store_unittest.h"

using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Test;
using ::testing::Values;

namespace shill {

class DeviceTest : public PropertyStoreTest {
 public:
  DeviceTest()
      : device_(new Device(control_interface(),
                           NULL,
                           NULL,
                           kDeviceName,
                           kDeviceAddress,
                           0)) {
    DHCPProvider::GetInstance()->glib_ = glib();
    DHCPProvider::GetInstance()->control_interface_ = control_interface();
  }
  virtual ~DeviceTest() {}

  virtual void SetUp() {
    device_->rtnl_handler_ = &rtnl_handler_;
  }

 protected:
  static const char kDeviceName[];
  static const char kDeviceAddress[];

  MockControl control_interface_;
  DeviceRefPtr device_;
  StrictMock<MockRTNLHandler> rtnl_handler_;
};

const char DeviceTest::kDeviceName[] = "testdevice";
const char DeviceTest::kDeviceAddress[] = "address";

TEST_F(DeviceTest, Contains) {
  EXPECT_TRUE(device_->store().Contains(flimflam::kNameProperty));
  EXPECT_FALSE(device_->store().Contains(""));
}

TEST_F(DeviceTest, GetProperties) {
  map<string, ::DBus::Variant> props;
  Error error(Error::kInvalidProperty, "");
  {
    ::DBus::Error dbus_error;
    bool expected = true;
    device_->mutable_store()->SetBoolProperty(flimflam::kPoweredProperty,
                                              expected,
                                              &error);
    DBusAdaptor::GetProperties(device_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kPoweredProperty) == props.end());
    EXPECT_EQ(props[flimflam::kPoweredProperty].reader().get_bool(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(device_->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kNameProperty) == props.end());
    EXPECT_EQ(props[flimflam::kNameProperty].reader().get_string(),
              string(kDeviceName));
  }
}

TEST_F(DeviceTest, Dispatch) {
  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_->mutable_store(),
                                          flimflam::kPoweredProperty,
                                          PropertyStoreTest::kBoolV,
                                          &error));

  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_->mutable_store(),
                                           flimflam::kAddressProperty,
                                           PropertyStoreTest::kStringV,
                                           &error));
  EXPECT_EQ(invalid_args(), error.name());
}

TEST_F(DeviceTest, TechnologyIs) {
  EXPECT_FALSE(device_->TechnologyIs(Technology::kEthernet));
}

TEST_F(DeviceTest, DestroyIPConfig) {
  ASSERT_FALSE(device_->ipconfig_.get());
  device_->ipconfig_ = new IPConfig(control_interface(), kDeviceName);
  device_->DestroyIPConfig();
  ASSERT_FALSE(device_->ipconfig_.get());
}

TEST_F(DeviceTest, DestroyIPConfigNULL) {
  ASSERT_FALSE(device_->ipconfig_.get());
  device_->DestroyIPConfig();
  ASSERT_FALSE(device_->ipconfig_.get());
}

TEST_F(DeviceTest, AcquireDHCPConfig) {
  device_->ipconfig_ = new IPConfig(control_interface(), "randomname");
  EXPECT_CALL(*glib(), SpawnAsync(_, _, _, _, _, _, _, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(device_->AcquireDHCPConfig());
  ASSERT_TRUE(device_->ipconfig_.get());
  EXPECT_EQ(kDeviceName, device_->ipconfig_->device_name());
  EXPECT_TRUE(device_->ipconfig_->update_callback_.get());
}

TEST_F(DeviceTest, Load) {
  NiceMock<MockStore> storage;
  const string id = device_->GetStorageIdentifier();
  EXPECT_CALL(storage, ContainsGroup(id)).WillOnce(Return(true));
  EXPECT_CALL(storage, GetBool(id, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(device_->Load(&storage));
}

TEST_F(DeviceTest, Save) {
  NiceMock<MockStore> storage;
  const string id = device_->GetStorageIdentifier();
  EXPECT_CALL(storage, SetString(id, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(storage, SetBool(id, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  scoped_refptr<MockIPConfig> ipconfig = new MockIPConfig(control_interface(),
                                                          kDeviceName);
  EXPECT_CALL(*ipconfig.get(), Save(_, _))
      .WillOnce(Return(true));
  device_->ipconfig_ = ipconfig;
  EXPECT_TRUE(device_->Save(&storage));
}

TEST_F(DeviceTest, StorageIdGeneration) {
  string to_process("/device/stuff/0");
  ControlInterface::RpcIdToStorageId(&to_process);
  EXPECT_TRUE(isalpha(to_process[0]));
  EXPECT_EQ(string::npos, to_process.find('/'));
}

TEST_F(DeviceTest, SelectedService) {
  EXPECT_FALSE(device_->selected_service_.get());
  device_->SetServiceState(Service::kStateAssociating);
  scoped_refptr<MockService> service(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  manager()));
  device_->SelectService(service);
  EXPECT_TRUE(device_->selected_service_.get() == service.get());

  EXPECT_CALL(*service.get(), SetState(Service::kStateConfiguring));
  device_->SetServiceState(Service::kStateConfiguring);
  EXPECT_CALL(*service.get(), SetFailure(Service::kFailureOutOfRange));
  device_->SetServiceFailure(Service::kFailureOutOfRange);

  // Service should be returned to "Idle" state
  EXPECT_CALL(*service.get(), state())
    .WillOnce(Return(Service::kStateUnknown));
  EXPECT_CALL(*service.get(), SetState(Service::kStateIdle));
  device_->SelectService(NULL);

  // A service in the "Failure" state should not be reset to "Idle"
  device_->SelectService(service);
  EXPECT_CALL(*service.get(), state())
    .WillOnce(Return(Service::kStateFailure));
  device_->SelectService(NULL);
}

TEST_F(DeviceTest, Stop) {
  device_->ipconfig_ = new IPConfig(&control_interface_, kDeviceName);
  scoped_refptr<MockService> service(
      new NiceMock<MockService>(&control_interface_,
                                dispatcher(),
                                manager()));
  device_->SelectService(service);

  EXPECT_CALL(*service.get(), state()).
      WillRepeatedly(Return(Service::kStateConnected));
  EXPECT_CALL(*dynamic_cast<DeviceMockAdaptor *>(device_->adaptor_.get()),
              UpdateEnabled());
  EXPECT_CALL(rtnl_handler_, SetInterfaceFlags(_, 0, IFF_UP));
  device_->Stop();

  EXPECT_FALSE(device_->ipconfig_.get());
  EXPECT_FALSE(device_->selected_service_.get());
}

}  // namespace shill
