// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager.h"

#include <set>

#include <glib.h>

#include <base/logging.h>
#include <base/stl_util-inl.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/adaptor_interfaces.h"
#include "shill/ephemeral_profile.h"
#include "shill/error.h"
#include "shill/glib.h"
#include "shill/key_file_store.h"
#include "shill/key_value_store.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_glib.h"
#include "shill/mock_profile.h"
#include "shill/mock_service.h"
#include "shill/mock_store.h"
#include "shill/mock_wifi.h"
#include "shill/property_store_unittest.h"
#include "shill/service_under_test.h"
#include "shill/wifi_service.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Test;

class ManagerTest : public PropertyStoreTest {
 public:
  ManagerTest()
      : mock_wifi_(new NiceMock<MockWiFi>(control_interface(),
                                          dispatcher(),
                                          manager(),
                                          "wifi0",
                                          "addr4",
                                          4)) {
    mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                     dispatcher(),
                                                     manager(),
                                                     "null0",
                                                     "addr0",
                                                     0));
    mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                     dispatcher(),
                                                     manager(),
                                                     "null1",
                                                     "addr1",
                                                     1));
    mock_devices_.push_back(new NiceMock<MockDevice>(control_interface(),
                                                     dispatcher(),
                                                     manager(),
                                                     "null2",
                                                     "addr2",
                                                     2));
  }
  virtual ~ManagerTest() {}

  bool IsDeviceRegistered(const DeviceRefPtr &device,
                          Technology::Identifier tech) {
    vector<DeviceRefPtr> devices;
    manager()->FilterByTechnology(tech, &devices);
    return (devices.size() == 1 && devices[0].get() == device.get());
  }
  bool ServiceOrderIs(ServiceRefPtr svc1, ServiceRefPtr svc2);

  Profile *CreateProfileForManager(Manager *manager, GLib *glib) {
    Profile::Identifier id("rather", "irrelevant");
    scoped_ptr<Profile> profile(new Profile(control_interface(),
                                            manager,
                                            id,
                                            "",
                                            false));
    FilePath final_path(storage_path());
    final_path = final_path.Append("test.profile");
    scoped_ptr<KeyFileStore> storage(new KeyFileStore(glib));
    storage->set_path(final_path);
    if (!storage->Open())
      return NULL;
    profile->set_storage(storage.release());  // Passes ownership.
    return profile.release();
  }

 protected:
  scoped_refptr<MockWiFi> mock_wifi_;
  vector<scoped_refptr<MockDevice> > mock_devices_;
};

bool ManagerTest::ServiceOrderIs(ServiceRefPtr svc0, ServiceRefPtr svc1) {
  return (svc0.get() == manager()->services_[0].get() &&
          svc1.get() == manager()->services_[1].get());
}

TEST_F(ManagerTest, Contains) {
  EXPECT_TRUE(manager()->store().Contains(flimflam::kStateProperty));
  EXPECT_FALSE(manager()->store().Contains(""));
}

TEST_F(ManagerTest, DeviceRegistration) {
  ON_CALL(*mock_devices_[0].get(), TechnologyIs(Technology::kEthernet))
      .WillByDefault(Return(true));
  ON_CALL(*mock_devices_[1].get(), TechnologyIs(Technology::kWifi))
      .WillByDefault(Return(true));
  ON_CALL(*mock_devices_[2].get(), TechnologyIs(Technology::kCellular))
      .WillByDefault(Return(true));

  manager()->RegisterDevice(mock_devices_[0]);
  manager()->RegisterDevice(mock_devices_[1]);
  manager()->RegisterDevice(mock_devices_[2]);

  EXPECT_TRUE(IsDeviceRegistered(mock_devices_[0], Technology::kEthernet));
  EXPECT_TRUE(IsDeviceRegistered(mock_devices_[1], Technology::kWifi));
  EXPECT_TRUE(IsDeviceRegistered(mock_devices_[2], Technology::kCellular));
}

TEST_F(ManagerTest, DeviceDeregistration) {
  ON_CALL(*mock_devices_[0].get(), TechnologyIs(Technology::kEthernet))
      .WillByDefault(Return(true));
  ON_CALL(*mock_devices_[1].get(), TechnologyIs(Technology::kWifi))
      .WillByDefault(Return(true));

  manager()->RegisterDevice(mock_devices_[0].get());
  manager()->RegisterDevice(mock_devices_[1].get());

  ASSERT_TRUE(IsDeviceRegistered(mock_devices_[0], Technology::kEthernet));
  ASSERT_TRUE(IsDeviceRegistered(mock_devices_[1], Technology::kWifi));

  EXPECT_CALL(*mock_devices_[0].get(), Stop());
  manager()->DeregisterDevice(mock_devices_[0].get());
  EXPECT_FALSE(IsDeviceRegistered(mock_devices_[0], Technology::kEthernet));

  EXPECT_CALL(*mock_devices_[1].get(), Stop());
  manager()->DeregisterDevice(mock_devices_[1].get());
  EXPECT_FALSE(IsDeviceRegistered(mock_devices_[1], Technology::kWifi));
}

TEST_F(ManagerTest, ServiceRegistration) {
  // It's much easier and safer to use a real GLib for this test.
  GLib glib;
  Manager manager(control_interface(),
                  dispatcher(),
                  &glib,
                  run_path(),
                  storage_path(),
                  string());
  ProfileRefPtr profile(CreateProfileForManager(&manager, &glib));
  ASSERT_TRUE(profile.get());
  manager.AdoptProfile(profile);

  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                &manager));
  scoped_refptr<MockService> mock_service2(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                &manager));
  string service1_name(mock_service->UniqueName());
  string service2_name(mock_service2->UniqueName());

  EXPECT_CALL(*mock_service.get(), GetRpcIdentifier())
      .WillRepeatedly(Return(service1_name));
  EXPECT_CALL(*mock_service2.get(), GetRpcIdentifier())
      .WillRepeatedly(Return(service2_name));
  // TODO(quiche): make this EXPECT_CALL work (crosbug.com/20154)
  // EXPECT_CALL(*dynamic_cast<ManagerMockAdaptor *>(manager.adaptor_.get()),
  //             EmitRpcIdentifierArrayChanged(flimflam::kServicesProperty, _));

  manager.RegisterService(mock_service);
  manager.RegisterService(mock_service2);

  vector<string> rpc_ids = manager.EnumerateAvailableServices();
  set<string> ids(rpc_ids.begin(), rpc_ids.end());
  EXPECT_EQ(2, ids.size());
  EXPECT_TRUE(ContainsKey(ids, mock_service->GetRpcIdentifier()));
  EXPECT_TRUE(ContainsKey(ids, mock_service2->GetRpcIdentifier()));

  EXPECT_TRUE(manager.FindService(service1_name).get() != NULL);
  EXPECT_TRUE(manager.FindService(service2_name).get() != NULL);

  manager.Stop();
}

TEST_F(ManagerTest, RegisterKnownService) {
  // It's much easier and safer to use a real GLib for this test.
  GLib glib;
  Manager manager(control_interface(),
                  dispatcher(),
                  &glib,
                  run_path(),
                  storage_path(),
                  string());
  ProfileRefPtr profile(CreateProfileForManager(&manager, &glib));
  ASSERT_TRUE(profile.get());
  manager.AdoptProfile(profile);
  {
    ServiceRefPtr service1(new ServiceUnderTest(control_interface(),
                                                dispatcher(),
                                                &manager));
    service1->set_favorite(!service1->favorite());
    ASSERT_TRUE(profile->AdoptService(service1));
    ASSERT_TRUE(profile->ContainsService(service1));
  }  // Force destruction of service1.

  ServiceRefPtr service2(new ServiceUnderTest(control_interface(),
                                              dispatcher(),
                                              &manager));
  manager.RegisterService(service2);
  EXPECT_EQ(service2->profile().get(), profile.get());
  manager.Stop();
}

TEST_F(ManagerTest, RegisterUnknownService) {
  // It's much easier and safer to use a real GLib for this test.
  GLib glib;
  Manager manager(control_interface(),
                  dispatcher(),
                  &glib,
                  run_path(),
                  storage_path(),
                  string());
  ProfileRefPtr profile(CreateProfileForManager(&manager, &glib));
  ASSERT_TRUE(profile.get());
  manager.AdoptProfile(profile);
  {
    ServiceRefPtr service1(new ServiceUnderTest(control_interface(),
                                                dispatcher(),
                                                &manager));
    service1->set_favorite(!service1->favorite());
    ASSERT_TRUE(profile->AdoptService(service1));
    ASSERT_TRUE(profile->ContainsService(service1));
  }  // Force destruction of service1.
  scoped_refptr<MockService> mock_service2(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                &manager));
  EXPECT_CALL(*mock_service2.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(mock_service2->UniqueName()));
  manager.RegisterService(mock_service2);
  EXPECT_NE(mock_service2->profile().get(), profile.get());
  manager.Stop();
}

TEST_F(ManagerTest, GetProperties) {
  ProfileRefPtr profile(new MockProfile(control_interface(), manager(), ""));
  manager()->AdoptProfile(profile);
  map<string, ::DBus::Variant> props;
  Error error(Error::kInvalidProperty, "");
  {
    ::DBus::Error dbus_error;
    string expected("portal_list");
    manager()->mutable_store()->SetStringProperty(
        flimflam::kCheckPortalListProperty,
        expected,
        &error);
    DBusAdaptor::GetProperties(manager()->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kCheckPortalListProperty) == props.end());
    EXPECT_EQ(props[flimflam::kCheckPortalListProperty].reader().get_string(),
              expected);
  }
  {
    ::DBus::Error dbus_error;
    bool expected = true;
    manager()->mutable_store()->SetBoolProperty(flimflam::kOfflineModeProperty,
                                                expected,
                                                &error);
    DBusAdaptor::GetProperties(manager()->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kOfflineModeProperty) == props.end());
    EXPECT_EQ(props[flimflam::kOfflineModeProperty].reader().get_bool(),
              expected);
  }
}

TEST_F(ManagerTest, GetDevicesProperty) {
  ProfileRefPtr profile(new MockProfile(control_interface(), manager(), ""));
  manager()->AdoptProfile(profile);
  manager()->RegisterDevice(mock_devices_[0].get());
  manager()->RegisterDevice(mock_devices_[1].get());
  {
    map<string, ::DBus::Variant> props;
    ::DBus::Error dbus_error;
    DBusAdaptor::GetProperties(manager()->store(), &props, &dbus_error);
    ASSERT_FALSE(props.find(flimflam::kDevicesProperty) == props.end());
    Strings devices =
        props[flimflam::kDevicesProperty].operator vector<string>();
    EXPECT_EQ(2, devices.size());
  }
}

TEST_F(ManagerTest, MoveService) {
  Manager manager(control_interface(),
                  dispatcher(),
                  glib(),
                  run_path(),
                  storage_path(),
                  string());
  scoped_refptr<MockService> s2(new MockService(control_interface(),
                                                dispatcher(),
                                                &manager));
  // Inject an actual profile, backed by a fake StoreInterface
  {
    Profile::Identifier id("irrelevant");
    ProfileRefPtr profile(
        new Profile(control_interface(), &manager, id, "", false));
    MockStore *storage = new MockStore;
    // Say we don't have |s2| the first time asked, then that we do.
    EXPECT_CALL(*storage, ContainsGroup(s2->GetStorageIdentifier()))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*storage, Flush())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    profile->set_storage(storage);
    manager.AdoptProfile(profile);
  }
  // Create a profile that already has |s2| in it.
  ProfileRefPtr profile(new EphemeralProfile(control_interface(), &manager));
  profile->AdoptService(s2);

  // Now, move the Service |s2| to another profile.
  EXPECT_CALL(*s2.get(), Save(_)).WillOnce(Return(true));
  ASSERT_TRUE(manager.MoveServiceToProfile(s2, manager.ActiveProfile()));

  // Force destruction of the original Profile, to ensure that the Service
  // is kept alive and populated with data.
  profile = NULL;
  ASSERT_TRUE(manager.ActiveProfile()->ContainsService(s2));
  manager.Stop();
}

TEST_F(ManagerTest, Dispatch) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(manager()->mutable_store(),
                                            flimflam::kOfflineModeProperty,
                                            PropertyStoreTest::kBoolV,
                                            &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(manager()->mutable_store(),
                                            flimflam::kCountryProperty,
                                            PropertyStoreTest::kStringV,
                                            &error));
  }
  // Attempt to write with value of wrong type should return InvalidArgs.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(manager()->mutable_store(),
                                             flimflam::kCountryProperty,
                                             PropertyStoreTest::kBoolV,
                                             &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(manager()->mutable_store(),
                                             flimflam::kOfflineModeProperty,
                                             PropertyStoreTest::kStringV,
                                             &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
  // Attempt to write R/O property should return InvalidArgs.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(
        manager()->mutable_store(),
        flimflam::kEnabledTechnologiesProperty,
        PropertyStoreTest::kStringsV,
        &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
}

TEST_F(ManagerTest, RequestScan) {
  {
    Error error;
    manager()->RegisterDevice(mock_devices_[0].get());
    manager()->RegisterDevice(mock_devices_[1].get());
    EXPECT_CALL(*mock_devices_[0], TechnologyIs(Technology::kWifi))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_devices_[0], Scan(_));
    EXPECT_CALL(*mock_devices_[1], TechnologyIs(Technology::kWifi))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_devices_[1], Scan(_)).Times(0);
    manager()->RequestScan(flimflam::kTypeWifi, &error);
  }

  {
    Error error;
    manager()->RequestScan("bogus_device_type", &error);
    EXPECT_EQ(Error::kInvalidArguments, error.type());
  }
}

TEST_F(ManagerTest, GetWifiServiceNoDevice) {
  KeyValueStore args;
  Error e;
  manager()->GetWifiService(args, &e);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_EQ("no wifi devices available", e.message());
}

TEST_F(ManagerTest, GetWifiService) {
  KeyValueStore args;
  Error e;
  WiFiServiceRefPtr wifi_service;

  manager()->RegisterDevice(mock_wifi_);
  EXPECT_CALL(*mock_wifi_, GetService(_, _))
      .WillRepeatedly(Return(wifi_service));
  manager()->GetWifiService(args, &e);
}

TEST_F(ManagerTest, TechnologyOrder) {
  Error error;
  manager()->SetTechnologyOrder(string(flimflam::kTypeEthernet) + "," +
                                string(flimflam::kTypeWifi), &error);
  ASSERT_TRUE(error.IsSuccess());
  EXPECT_EQ(manager()->GetTechnologyOrder(),
            string(flimflam::kTypeEthernet) + "," +
            string(flimflam::kTypeWifi));

  manager()->SetTechnologyOrder(string(flimflam::kTypeEthernet) + "x," +
                                string(flimflam::kTypeWifi), &error);
  ASSERT_FALSE(error.IsSuccess());
  EXPECT_EQ(Error::kInvalidArguments, error.type());
  EXPECT_EQ(string(flimflam::kTypeEthernet) + "," +
            string(flimflam::kTypeWifi),
            manager()->GetTechnologyOrder());
}

TEST_F(ManagerTest, SortServices) {
  scoped_refptr<MockService> mock_service0(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                manager()));
  scoped_refptr<MockService> mock_service1(
      new NiceMock<MockService>(control_interface(),
                                dispatcher(),
                                manager()));
  string service1_name(mock_service0->UniqueName());
  string service2_name(mock_service1->UniqueName());

  manager()->RegisterService(mock_service0);
  manager()->RegisterService(mock_service1);

  // Services should already be sorted by UniqueName
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Asking explictly to sort services should not change anything
  manager()->SortServices();
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Two otherwise equal services should be reordered by strength
  mock_service1->set_strength(1);
  manager()->UpdateService(mock_service1);
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  // Security
  mock_service0->set_security(1);
  manager()->UpdateService(mock_service0);
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Technology
  EXPECT_CALL(*mock_service0.get(), TechnologyIs(Technology::kWifi))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_service1.get(), TechnologyIs(Technology::kEthernet))
      .WillRepeatedly(Return(true));
  // NB: Redefine default (false) return values so we don't use the default rule
  // which makes the logs noisier
  EXPECT_CALL(*mock_service0.get(), TechnologyIs(Ne(Technology::kWifi)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_service1.get(), TechnologyIs(Ne(Technology::kEthernet)))
      .WillRepeatedly(Return(false));

  Error error;
  manager()->SetTechnologyOrder(string(flimflam::kTypeEthernet) + "," +
                                string(flimflam::kTypeWifi), &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  manager()->SetTechnologyOrder(string(flimflam::kTypeWifi) + "," +
                                string(flimflam::kTypeEthernet), &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Priority
  mock_service0->set_priority(1);
  manager()->UpdateService(mock_service0);
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Favorite
  mock_service1->set_favorite(true);
  manager()->UpdateService(mock_service1);
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  // Connecting
  EXPECT_CALL(*mock_service0.get(), state())
      .WillRepeatedly(Return(Service::kStateAssociating));
  manager()->UpdateService(mock_service0);
  EXPECT_TRUE(ServiceOrderIs(mock_service0, mock_service1));

  // Connected
  EXPECT_CALL(*mock_service1.get(), state())
      .WillRepeatedly(Return(Service::kStateConnected));
  manager()->UpdateService(mock_service1);
  EXPECT_TRUE(ServiceOrderIs(mock_service1, mock_service0));

  manager()->DeregisterService(mock_service0);
  manager()->DeregisterService(mock_service1);
}

}  // namespace shill
