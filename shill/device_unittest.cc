// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"
#include "shill/mock_connection.h"
#include "shill/mock_device.h"
#include "shill/mock_device_info.h"
#include "shill/mock_glib.h"
#include "shill/mock_ipconfig.h"
#include "shill/mock_manager.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/mock_service.h"
#include "shill/mock_store.h"
#include "shill/portal_detector.h"
#include "shill/property_store_unittest.h"
#include "shill/technology.h"

using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::Test;
using ::testing::Values;

namespace shill {

class DeviceTest : public PropertyStoreTest {
 public:
  DeviceTest()
      : device_(new Device(control_interface(),
                           dispatcher(),
                           NULL,
                           manager(),
                           kDeviceName,
                           kDeviceAddress,
                           0,
                           Technology::kUnknown)) {
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

  void IPConfigUpdatedCallback(const IPConfigRefPtr &ipconfig, bool success) {
    device_->IPConfigUpdatedCallback(ipconfig, success);
  }

  void SelectService(const ServiceRefPtr service) {
    device_->SelectService(service);
  }

  bool StartPortalDetection() { return device_->StartPortalDetection(); }
  void StopPortalDetection() { device_->StopPortalDetection(); }

  void PortalDetectorCallback(const PortalDetector::Result &result) {
    device_->PortalDetectorCallback(result);
  }

  void SetManager(Manager *manager) {
    device_->manager_ = manager;
  }

  void SetConnection(ConnectionRefPtr connection) {
    device_->connection_ = connection;
  }

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

TEST_F(DeviceTest, SetProperty) {
  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                       flimflam::kPoweredProperty,
                                       PropertyStoreTest::kBoolV,
                                       &error));

  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  EXPECT_FALSE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                        flimflam::kAddressProperty,
                                        PropertyStoreTest::kStringV,
                                        &error));
  EXPECT_EQ(invalid_args(), error.name());
}

TEST_F(DeviceTest, ClearProperty) {
  ::DBus::Error error;
  EXPECT_TRUE(device_->powered());

  EXPECT_TRUE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                       flimflam::kPoweredProperty,
                                       PropertyStoreTest::kBoolV,
                                       &error));
  EXPECT_FALSE(device_->powered());

  EXPECT_TRUE(DBusAdaptor::ClearProperty(device_->mutable_store(),
                                         flimflam::kPoweredProperty,
                                         &error));
  EXPECT_TRUE(device_->powered());
}

TEST_F(DeviceTest, ClearReadOnlyProperty) {
  ::DBus::Error error;
  EXPECT_FALSE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                        flimflam::kAddressProperty,
                                        PropertyStoreTest::kStringV,
                                        &error));
}

TEST_F(DeviceTest, ClearReadOnlyDerivedProperty) {
  ::DBus::Error error;
  EXPECT_FALSE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                        flimflam::kIPConfigsProperty,
                                        PropertyStoreTest::kStringsV,
                                        &error));
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

TEST_F(DeviceTest, AcquireIPConfig) {
  device_->ipconfig_ = new IPConfig(control_interface(), "randomname");
  EXPECT_CALL(*glib(), SpawnAsync(_, _, _, _, _, _, _, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(device_->AcquireIPConfig());
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

MATCHER(IsNullRefPtr, "") {
  return !arg;
}

MATCHER(NotNullRefPtr, "") {
  return arg;
}

TEST_F(DeviceTest, SelectedService) {
  EXPECT_FALSE(device_->selected_service_.get());
  device_->SetServiceState(Service::kStateAssociating);
  scoped_refptr<MockService> service(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  manager()));
  SelectService(service);
  EXPECT_TRUE(device_->selected_service_.get() == service.get());

  EXPECT_CALL(*service.get(), SetState(Service::kStateConfiguring));
  device_->SetServiceState(Service::kStateConfiguring);
  EXPECT_CALL(*service.get(), SetFailure(Service::kFailureOutOfRange));
  device_->SetServiceFailure(Service::kFailureOutOfRange);

  // Service should be returned to "Idle" state
  EXPECT_CALL(*service.get(), state())
    .WillOnce(Return(Service::kStateUnknown));
  EXPECT_CALL(*service.get(), SetState(Service::kStateIdle));
  EXPECT_CALL(*service.get(), SetConnection(IsNullRefPtr()));
  SelectService(NULL);

  // A service in the "Failure" state should not be reset to "Idle"
  SelectService(service);
  EXPECT_CALL(*service.get(), state())
    .WillOnce(Return(Service::kStateFailure));
  EXPECT_CALL(*service.get(), SetConnection(IsNullRefPtr()));
  SelectService(NULL);
}

TEST_F(DeviceTest, IPConfigUpdatedFailure) {
  scoped_refptr<MockService> service(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  manager()));
  SelectService(service);
  EXPECT_CALL(*service.get(), SetState(Service::kStateDisconnected));
  EXPECT_CALL(*service.get(), SetConnection(IsNullRefPtr()));
  IPConfigUpdatedCallback(NULL, false);
}

TEST_F(DeviceTest, IPConfigUpdatedSuccess) {
  scoped_refptr<MockService> service(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  manager()));
  SelectService(service);
  scoped_refptr<MockIPConfig> ipconfig = new MockIPConfig(control_interface(),
                                                          kDeviceName);
  EXPECT_CALL(*service.get(), SetState(Service::kStateConnected));
  EXPECT_CALL(*service.get(), IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*service.get(), SetState(Service::kStateOnline));
  EXPECT_CALL(*service.get(), SetConnection(NotNullRefPtr()));
  IPConfigUpdatedCallback(ipconfig.get(), true);
}

TEST_F(DeviceTest, Stop) {
  device_->ipconfig_ = new IPConfig(&control_interface_, kDeviceName);
  scoped_refptr<MockService> service(
      new NiceMock<MockService>(&control_interface_,
                                dispatcher(),
                                metrics(),
                                manager()));
  SelectService(service);

  EXPECT_CALL(*service.get(), state()).
      WillRepeatedly(Return(Service::kStateConnected));
  EXPECT_CALL(*dynamic_cast<DeviceMockAdaptor *>(device_->adaptor_.get()),
              UpdateEnabled());
  EXPECT_CALL(rtnl_handler_, SetInterfaceFlags(_, 0, IFF_UP));
  device_->Stop();

  EXPECT_FALSE(device_->ipconfig_.get());
  EXPECT_FALSE(device_->selected_service_.get());
}

TEST_F(DeviceTest, PortalDetectionDisabled) {
  scoped_refptr<MockService> service(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  manager()));
  EXPECT_CALL(*service.get(), IsConnected())
      .WillRepeatedly(Return(true));
  StrictMock<MockManager> manager(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  glib());
  SetManager(&manager);
  EXPECT_CALL(manager, IsPortalDetectionEnabled(device_->technology()))
      .WillOnce(Return(false));
  SelectService(service);
  EXPECT_CALL(*service.get(), SetState(Service::kStateOnline));
  EXPECT_FALSE(StartPortalDetection());
}

TEST_F(DeviceTest, PortalDetectionProxyConfig) {
  scoped_refptr<MockService> service(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  manager()));
  EXPECT_CALL(*service.get(), IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*service.get(), HasProxyConfig())
      .WillOnce(Return(true));
  SelectService(service);
  StrictMock<MockManager> manager(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  glib());
  EXPECT_CALL(manager, IsPortalDetectionEnabled(device_->technology()))
      .WillOnce(Return(true));
  SetManager(&manager);
  EXPECT_CALL(*service.get(), SetState(Service::kStateOnline));
  EXPECT_FALSE(StartPortalDetection());
}

TEST_F(DeviceTest, PortalDetectionBadUrl) {
  scoped_refptr<MockService> service(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  manager()));
  EXPECT_CALL(*service.get(), IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*service.get(), HasProxyConfig())
      .WillOnce(Return(false));
  SelectService(service);
  StrictMock<MockManager> manager(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  glib());
  EXPECT_CALL(manager, IsPortalDetectionEnabled(device_->technology()))
      .WillOnce(Return(true));
  const string portal_url;
  EXPECT_CALL(manager, GetPortalCheckURL())
      .WillRepeatedly(ReturnRef(portal_url));
  SetManager(&manager);
  EXPECT_CALL(*service.get(), SetState(Service::kStateOnline));
  EXPECT_FALSE(StartPortalDetection());
}

TEST_F(DeviceTest, PortalDetectionStart) {
  scoped_refptr<MockService> service(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  manager()));
  EXPECT_CALL(*service.get(), IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*service.get(), HasProxyConfig())
      .WillOnce(Return(false));
  SelectService(service);
  StrictMock<MockManager> manager(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  glib());
  EXPECT_CALL(manager, IsPortalDetectionEnabled(device_->technology()))
      .WillOnce(Return(true));
  const string portal_url(PortalDetector::kDefaultURL);
  EXPECT_CALL(manager, GetPortalCheckURL())
      .WillRepeatedly(ReturnRef(portal_url));
  SetManager(&manager);
  EXPECT_CALL(*service.get(), SetState(Service::kStateOnline))
      .Times(0);
  scoped_ptr<MockDeviceInfo> device_info(
      new NiceMock<MockDeviceInfo>(
          control_interface(),
          reinterpret_cast<EventDispatcher *>(NULL),
          reinterpret_cast<Metrics *>(NULL),
          reinterpret_cast<Manager *>(NULL)));
  scoped_refptr<MockConnection> connection(
      new NiceMock<MockConnection>(device_info.get()));
  const string kInterfaceName("int0");
  EXPECT_CALL(*connection.get(), interface_name())
              .WillRepeatedly(ReturnRef(kInterfaceName));
  const vector<string> kDNSServers;
  EXPECT_CALL(*connection.get(), dns_servers())
              .WillRepeatedly(ReturnRef(kDNSServers));
  SetConnection(connection.get());
  EXPECT_TRUE(StartPortalDetection());

  // Drop all references to device_info before it falls out of scope.
  SetConnection(NULL);
  StopPortalDetection();
}

TEST_F(DeviceTest, PortalDetectionNonFinal) {
  scoped_refptr<MockService> service(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  manager()));
  EXPECT_CALL(*service.get(), IsConnected())
      .Times(0);
  EXPECT_CALL(*service.get(), SetState(_))
      .Times(0);
  SelectService(service);
  PortalDetectorCallback(PortalDetector::Result(
      PortalDetector::kPhaseUnknown,
      PortalDetector::kStatusFailure,
      false));
}

TEST_F(DeviceTest, PortalDetectionFailure) {
  scoped_refptr<MockService> service(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  manager()));
  EXPECT_CALL(*service.get(), IsConnected())
      .WillOnce(Return(true));
  SelectService(service);
  EXPECT_CALL(*service.get(), SetState(Service::kStatePortal));
  PortalDetectorCallback(PortalDetector::Result(
      PortalDetector::kPhaseUnknown,
      PortalDetector::kStatusFailure,
      true));
}

TEST_F(DeviceTest, PortalDetectionSuccess) {
  scoped_refptr<MockService> service(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  metrics(),
                                  manager()));
  EXPECT_CALL(*service.get(), IsConnected())
      .WillOnce(Return(true));
  SelectService(service);
  EXPECT_CALL(*service.get(), SetState(Service::kStateOnline));
  PortalDetectorCallback(PortalDetector::Result(
      PortalDetector::kPhaseUnknown,
      PortalDetector::kStatusSuccess,
      true));
}

}  // namespace shill
