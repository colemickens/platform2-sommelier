// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager.h"

#include <set>

#include <glib.h>

#include <base/logging.h>
#include <base/stl_util-inl.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_glib.h"
#include "shill/mock_profile.h"
#include "shill/mock_service.h"
#include "shill/property_store_unittest.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {
using ::testing::Test;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class ManagerTest : public PropertyStoreTest {
 public:
  ManagerTest()
      : mock_device_(new NiceMock<MockDevice>(&control_interface_,
                                              &dispatcher_,
                                              &manager_,
                                              "null0",
                                              "addr0",
                                              0)),
        mock_device2_(new NiceMock<MockDevice>(&control_interface_,
                                               &dispatcher_,
                                               &manager_,
                                               "null2",
                                               "addr2",
                                               2)),
        mock_device3_(new NiceMock<MockDevice>(&control_interface_,
                                               &dispatcher_,
                                               &manager_,
                                               "null3",
                                               "addr3",
                                               3)) {
  }
  virtual ~ManagerTest() {}

  bool IsDeviceRegistered(const DeviceRefPtr &device, Device::Technology tech) {
    vector<DeviceRefPtr> devices;
    manager_.FilterByTechnology(tech, &devices);
    return (devices.size() == 1 && devices[0].get() == device.get());
  }

 protected:
  scoped_refptr<MockDevice> mock_device_;
  scoped_refptr<MockDevice> mock_device2_;
  scoped_refptr<MockDevice> mock_device3_;
};

TEST_F(ManagerTest, Contains) {
  EXPECT_TRUE(manager_.store()->Contains(flimflam::kStateProperty));
  EXPECT_FALSE(manager_.store()->Contains(""));
}

TEST_F(ManagerTest, DeviceRegistration) {
  ON_CALL(*mock_device_.get(), TechnologyIs(Device::kEthernet))
      .WillByDefault(Return(true));
  ON_CALL(*mock_device2_.get(), TechnologyIs(Device::kWifi))
      .WillByDefault(Return(true));
  ON_CALL(*mock_device3_.get(), TechnologyIs(Device::kCellular))
      .WillByDefault(Return(true));

  manager_.RegisterDevice(mock_device_);
  manager_.RegisterDevice(mock_device2_);
  manager_.RegisterDevice(mock_device3_);

  EXPECT_TRUE(IsDeviceRegistered(mock_device_, Device::kEthernet));
  EXPECT_TRUE(IsDeviceRegistered(mock_device2_, Device::kWifi));
  EXPECT_TRUE(IsDeviceRegistered(mock_device3_, Device::kCellular));
}

TEST_F(ManagerTest, DeviceDeregistration) {
  ON_CALL(*mock_device_.get(), TechnologyIs(Device::kEthernet))
      .WillByDefault(Return(true));
  ON_CALL(*mock_device2_.get(), TechnologyIs(Device::kWifi))
      .WillByDefault(Return(true));

  manager_.RegisterDevice(mock_device_.get());
  manager_.RegisterDevice(mock_device2_.get());

  ASSERT_TRUE(IsDeviceRegistered(mock_device_, Device::kEthernet));
  ASSERT_TRUE(IsDeviceRegistered(mock_device2_, Device::kWifi));

  manager_.DeregisterDevice(mock_device_.get());
  EXPECT_FALSE(IsDeviceRegistered(mock_device_, Device::kEthernet));

  manager_.DeregisterDevice(mock_device2_.get());
  EXPECT_FALSE(IsDeviceRegistered(mock_device2_, Device::kWifi));
}

TEST_F(ManagerTest, ServiceRegistration) {
  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(&control_interface_,
                                &dispatcher_,
                                &manager_));
  scoped_refptr<MockService> mock_service2(
      new NiceMock<MockService>(&control_interface_,
                                &dispatcher_,
                                &manager_));
  string service1_name(mock_service->UniqueName());
  string service2_name(mock_service2->UniqueName());

  EXPECT_CALL(*mock_service.get(), GetRpcIdentifier())
      .WillRepeatedly(Return(service1_name));
  EXPECT_CALL(*mock_service2.get(), GetRpcIdentifier())
      .WillRepeatedly(Return(service2_name));

  manager_.RegisterService(mock_service);
  manager_.RegisterService(mock_service2);

  vector<string> rpc_ids = manager_.EnumerateAvailableServices();
  set<string> ids(rpc_ids.begin(), rpc_ids.end());
  EXPECT_EQ(2, ids.size());
  EXPECT_TRUE(ContainsKey(ids, mock_service->GetRpcIdentifier()));
  EXPECT_TRUE(ContainsKey(ids, mock_service2->GetRpcIdentifier()));

  EXPECT_TRUE(manager_.FindService(service1_name).get() != NULL);
  EXPECT_TRUE(manager_.FindService(service2_name).get() != NULL);
}

TEST_F(ManagerTest, GetProperties) {
  map<string, ::DBus::Variant> props;
  Error error(Error::kInvalidProperty, "");
  {
    ::DBus::Error dbus_error;
    string expected("portal_list");
    manager_.store()->SetStringProperty(flimflam::kCheckPortalListProperty,
                                        expected,
                                        &error);
    DBusAdaptor::GetProperties(manager_.store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kCheckPortalListProperty) == props.end());
    EXPECT_EQ(props[flimflam::kCheckPortalListProperty].reader().get_string(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    bool expected = true;
    manager_.store()->SetBoolProperty(flimflam::kOfflineModeProperty,
                                      expected,
                                      &error);
    DBusAdaptor::GetProperties(manager_.store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kOfflineModeProperty) == props.end());
    EXPECT_EQ(props[flimflam::kOfflineModeProperty].reader().get_bool(),
              expected);
  }
}

TEST_F(ManagerTest, GetDevicesProperty) {
  manager_.RegisterDevice(mock_device_.get());
  manager_.RegisterDevice(mock_device2_.get());
  {
    map<string, ::DBus::Variant> props;
    ::DBus::Error dbus_error;
    bool expected = true;
    DBusAdaptor::GetProperties(manager_.store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kDevicesProperty) == props.end());
    Strings devices =
        props[flimflam::kDevicesProperty].operator vector<string>();
    EXPECT_EQ(2, devices.size());
  }
}

TEST_F(ManagerTest, MoveService) {
  // I want to ensure that the Profiles are managing this Service object
  // lifetime properly, so I can't hold a ref to it here.
  ProfileRefPtr profile(
      new MockProfile(&control_interface_, &glib_, &manager_, ""));
  string service_name;
  {
    ServiceRefPtr s2(
        new MockService(&control_interface_,
                        &dispatcher_,
                        &manager_));
    profile->AdoptService(s2);
    s2->set_profile(profile);
    service_name = s2->UniqueName();
  }

  // Now, move the |service| to another profile.
  manager_.MoveToActiveProfile(profile, profile->FindService(service_name));

  // Force destruction of the original Profile, to ensure that the Service
  // is kept alive and populated with data.
  profile = NULL;
  {
    ServiceRefPtr serv(manager_.ActiveProfile()->FindService(service_name));
    ASSERT_TRUE(serv.get() != NULL);
    Error error(Error::kInvalidProperty, "");
    ::DBus::Error dbus_error;
    map<string, ::DBus::Variant> props;
    bool expected = true;
    serv->store()->SetBoolProperty(flimflam::kAutoConnectProperty,
                                   expected,
                                   &error);
    DBusAdaptor::GetProperties(serv->store(), &props, &dbus_error);
    ASSERT_TRUE(ContainsKey(props, flimflam::kAutoConnectProperty));
    EXPECT_EQ(props[flimflam::kAutoConnectProperty].reader().get_bool(),
              expected);
  }
}

TEST_F(ManagerTest, Dispatch) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(manager_.store(),
                                            flimflam::kOfflineModeProperty,
                                            PropertyStoreTest::kBoolV,
                                            &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(manager_.store(),
                                            flimflam::kCountryProperty,
                                            PropertyStoreTest::kStringV,
                                            &error));
  }
  // Attempt to write with value of wrong type should return InvalidArgs.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(manager_.store(),
                                             flimflam::kCountryProperty,
                                             PropertyStoreTest::kBoolV,
                                             &error));
    EXPECT_EQ(invalid_args_, error.name());
  }
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(manager_.store(),
                                             flimflam::kOfflineModeProperty,
                                             PropertyStoreTest::kStringV,
                                             &error));
    EXPECT_EQ(invalid_args_, error.name());
  }
  // Attempt to write R/O property should return InvalidArgs.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(
        manager_.store(),
        flimflam::kEnabledTechnologiesProperty,
        PropertyStoreTest::kStringsV,
        &error));
    EXPECT_EQ(invalid_args_, error.name());
  }
}

}  // namespace shill
