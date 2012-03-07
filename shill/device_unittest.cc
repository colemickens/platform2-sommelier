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
#include "shill/mock_metrics.h"
#include "shill/mock_portal_detector.h"
#include "shill/mock_routing_table.h"
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
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::Test;
using ::testing::Values;

namespace shill {

class TestDevice : public Device {
 public:
  TestDevice(ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             Metrics *metrics,
             Manager *manager,
             const std::string &link_name,
             const std::string &address,
             int interface_index,
             Technology::Identifier technology)
      : Device(control_interface, dispatcher, metrics, manager, link_name,
               address, interface_index, technology) {}
  ~TestDevice() {}
  virtual void Start(Error *error,
                     const EnabledStateChangedCallback &callback) {}
  virtual void Stop(Error *error,
                    const EnabledStateChangedCallback &callback) {}
};

class DeviceTest : public PropertyStoreTest {
 public:
  DeviceTest()
      : device_(new TestDevice(control_interface(),
                               dispatcher(),
                               NULL,
                               manager(),
                               kDeviceName,
                               kDeviceAddress,
                               kDeviceInterfaceIndex,
                               Technology::kUnknown)),
        device_info_(control_interface(), NULL, NULL, NULL) {
    DHCPProvider::GetInstance()->glib_ = glib();
    DHCPProvider::GetInstance()->control_interface_ = control_interface();
  }
  virtual ~DeviceTest() {}

  virtual void SetUp() {
    device_->metrics_ = &metrics_;
    device_->routing_table_ = &routing_table_;
    device_->rtnl_handler_ = &rtnl_handler_;
  }

 protected:
  static const char kDeviceName[];
  static const char kDeviceAddress[];
  static const int kDeviceInterfaceIndex;

  void OnIPConfigUpdated(const IPConfigRefPtr &ipconfig, bool success) {
    device_->OnIPConfigUpdated(ipconfig, success);
  }

  void SelectService(const ServiceRefPtr service) {
    device_->SelectService(service);
  }

  void SetConnection(ConnectionRefPtr connection) {
    device_->connection_ = connection;
  }

  MockControl control_interface_;
  DeviceRefPtr device_;
  MockDeviceInfo device_info_;
  MockMetrics metrics_;
  MockRoutingTable routing_table_;
  StrictMock<MockRTNLHandler> rtnl_handler_;
};

const char DeviceTest::kDeviceName[] = "testdevice";
const char DeviceTest::kDeviceAddress[] = "address";
const int DeviceTest::kDeviceInterfaceIndex = 0;

TEST_F(DeviceTest, Contains) {
  EXPECT_TRUE(device_->store().Contains(flimflam::kNameProperty));
  EXPECT_FALSE(device_->store().Contains(""));
}

TEST_F(DeviceTest, GetProperties) {
  map<string, ::DBus::Variant> props;
  Error error(Error::kInvalidProperty, "");
  ::DBus::Error dbus_error;
  DBusAdaptor::GetProperties(device_->store(), &props, &dbus_error);
  ASSERT_FALSE(props.find(flimflam::kNameProperty) == props.end());
  EXPECT_EQ(props[flimflam::kNameProperty].reader().get_string(),
            string(kDeviceName));
}

// Note: there are currently no writeable Device properties that
// aren't registered in a subclass.
TEST_F(DeviceTest, SetReadOnlyProperty) {
  ::DBus::Error error;
  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  EXPECT_FALSE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                        flimflam::kAddressProperty,
                                        PropertyStoreTest::kStringV,
                                        &error));
  EXPECT_EQ(invalid_args(), error.name());
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
  EXPECT_FALSE(device_->ipconfig_->update_callback_.is_null());
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
  OnIPConfigUpdated(NULL, false);
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
  OnIPConfigUpdated(ipconfig.get(), true);
}

TEST_F(DeviceTest, Start) {
  EXPECT_CALL(routing_table_, FlushRoutes(kDeviceInterfaceIndex));
  device_->SetEnabled(true);
}

TEST_F(DeviceTest, Stop) {
  device_->enabled_ = true;
  device_->enabled_pending_ = true;
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
  EXPECT_CALL(*dynamic_cast<DeviceMockAdaptor *>(device_->adaptor_.get()),
              EmitBoolChanged(flimflam::kPoweredProperty, false));
  EXPECT_CALL(rtnl_handler_, SetInterfaceFlags(_, 0, IFF_UP));
  device_->SetEnabled(false);
  device_->OnEnabledStateChanged(ResultCallback(), Error());

  EXPECT_FALSE(device_->ipconfig_.get());
  EXPECT_FALSE(device_->selected_service_.get());
}


class DevicePortalDetectionTest : public DeviceTest {
 public:
  DevicePortalDetectionTest()
      : connection_(new StrictMock<MockConnection>(&device_info_)),
        manager_(control_interface(),
                 dispatcher(),
                 metrics(),
                 glib()),
        service_(new StrictMock<MockService>(control_interface(),
                                             dispatcher(),
                                             metrics(),
                                             &manager_)),
        portal_detector_(new StrictMock<MockPortalDetector>(connection_)) {}
    virtual ~DevicePortalDetectionTest() {}
  virtual void SetUp() {
    DeviceTest::SetUp();
    SelectService(service_);
    SetConnection(connection_.get());
    device_->portal_detector_.reset(portal_detector_);  // Passes ownership.
    device_->manager_ = &manager_;
  }

 protected:
  static const int kPortalAttempts;

  bool StartPortalDetection() { return device_->StartPortalDetection(); }
  void StopPortalDetection() { device_->StopPortalDetection(); }

  void PortalDetectorCallback(const PortalDetector::Result &result) {
    device_->PortalDetectorCallback(result);
  }
  bool RequestPortalDetection() {
    return device_->RequestPortalDetection();
  }
  void SetServiceConnectedState(Service::ConnectState state) {
    device_->SetServiceConnectedState(state);
  }
  void ExpectPortalDetectorReset() {
    EXPECT_FALSE(device_->portal_detector_.get());
  }
  void ExpectPortalDetectorSet() {
    EXPECT_TRUE(device_->portal_detector_.get());
  }
  void ExpectPortalDetectorIsMock() {
    EXPECT_EQ(portal_detector_, device_->portal_detector_.get());
  }
  scoped_refptr<MockConnection> connection_;
  StrictMock<MockManager> manager_;
  scoped_refptr<MockService> service_;

  // Used only for EXPECT_CALL().  Object is owned by device.
  MockPortalDetector *portal_detector_;
};

const int DevicePortalDetectionTest::kPortalAttempts = 2;

TEST_F(DevicePortalDetectionTest, PortalDetectionDisabled) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(manager_, IsPortalDetectionEnabled(device_->technology()))
      .WillOnce(Return(false));
  EXPECT_CALL(*service_.get(), SetState(Service::kStateOnline));
  EXPECT_FALSE(StartPortalDetection());
}

TEST_F(DevicePortalDetectionTest, PortalDetectionProxyConfig) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*service_.get(), HasProxyConfig())
      .WillOnce(Return(true));
  EXPECT_CALL(manager_, IsPortalDetectionEnabled(device_->technology()))
      .WillOnce(Return(true));
  EXPECT_CALL(*service_.get(), SetState(Service::kStateOnline));
  EXPECT_FALSE(StartPortalDetection());
}

TEST_F(DevicePortalDetectionTest, PortalDetectionBadUrl) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*service_.get(), HasProxyConfig())
      .WillOnce(Return(false));
  EXPECT_CALL(manager_, IsPortalDetectionEnabled(device_->technology()))
      .WillOnce(Return(true));
  const string portal_url;
  EXPECT_CALL(manager_, GetPortalCheckURL())
      .WillRepeatedly(ReturnRef(portal_url));
  EXPECT_CALL(*service_.get(), SetState(Service::kStateOnline));
  EXPECT_FALSE(StartPortalDetection());
}

TEST_F(DevicePortalDetectionTest, PortalDetectionStart) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*service_.get(), HasProxyConfig())
      .WillOnce(Return(false));
  EXPECT_CALL(manager_, IsPortalDetectionEnabled(device_->technology()))
      .WillOnce(Return(true));
  const string portal_url(PortalDetector::kDefaultURL);
  EXPECT_CALL(manager_, GetPortalCheckURL())
      .WillRepeatedly(ReturnRef(portal_url));
  EXPECT_CALL(*service_.get(), SetState(Service::kStateOnline))
      .Times(0);
  const string kInterfaceName("int0");
  EXPECT_CALL(*connection_.get(), interface_name())
      .WillRepeatedly(ReturnRef(kInterfaceName));
  const vector<string> kDNSServers;
  EXPECT_CALL(*connection_.get(), dns_servers())
      .WillRepeatedly(ReturnRef(kDNSServers));
  EXPECT_TRUE(StartPortalDetection());

  // Drop all references to device_info before it falls out of scope.
  SetConnection(NULL);
  StopPortalDetection();
}

TEST_F(DevicePortalDetectionTest, PortalDetectionNonFinal) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .Times(0);
  EXPECT_CALL(*service_.get(), SetState(_))
      .Times(0);
  PortalDetectorCallback(PortalDetector::Result(
      PortalDetector::kPhaseUnknown,
      PortalDetector::kStatusFailure,
      kPortalAttempts,
      false));
}

TEST_F(DevicePortalDetectionTest, PortalDetectionFailure) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillOnce(Return(true));
  EXPECT_CALL(*service_.get(), SetState(Service::kStatePortal));
  EXPECT_CALL(metrics_,
              SendEnumToUMA("Network.Shill.Unknown.PortalResult",
                            Metrics::kPortalResultConnectionFailure,
                            Metrics::kPortalResultMax));
  EXPECT_CALL(metrics_,
              SendToUMA("Network.Shill.Unknown.PortalAttemptsToOnline",
                        _, _, _, _)).Times(0);
  EXPECT_CALL(metrics_,
              SendToUMA("Network.Shill.Unknown.PortalAttempts",
                        kPortalAttempts,
                        Metrics::kMetricPortalAttemptsMin,
                        Metrics::kMetricPortalAttemptsMax,
                        Metrics::kMetricPortalAttemptsNumBuckets));
  EXPECT_CALL(*connection_.get(), is_default())
      .WillOnce(Return(false));
  PortalDetectorCallback(PortalDetector::Result(
      PortalDetector::kPhaseConnection,
      PortalDetector::kStatusFailure,
      kPortalAttempts,
      true));
}

TEST_F(DevicePortalDetectionTest, PortalDetectionSuccess) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillOnce(Return(true));
  EXPECT_CALL(*service_.get(), SetState(Service::kStateOnline));
  EXPECT_CALL(metrics_,
              SendEnumToUMA("Network.Shill.Unknown.PortalResult",
                            Metrics::kPortalResultSuccess,
                            Metrics::kPortalResultMax));
  EXPECT_CALL(metrics_,
              SendToUMA("Network.Shill.Unknown.PortalAttemptsToOnline",
                        kPortalAttempts,
                        Metrics::kMetricPortalAttemptsToOnlineMin,
                        Metrics::kMetricPortalAttemptsToOnlineMax,
                        Metrics::kMetricPortalAttemptsToOnlineNumBuckets));
  EXPECT_CALL(metrics_,
              SendToUMA("Network.Shill.Unknown.PortalAttempts",
                        _, _, _, _)).Times(0);
  PortalDetectorCallback(PortalDetector::Result(
      PortalDetector::kPhaseContent,
      PortalDetector::kStatusSuccess,
      kPortalAttempts,
      true));
}

TEST_F(DevicePortalDetectionTest, PortalDetectionSuccessAfterFailure) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*service_.get(), SetState(Service::kStatePortal));
  EXPECT_CALL(metrics_,
              SendEnumToUMA("Network.Shill.Unknown.PortalResult",
                            Metrics::kPortalResultConnectionFailure,
                            Metrics::kPortalResultMax));
  EXPECT_CALL(metrics_,
              SendToUMA("Network.Shill.Unknown.PortalAttemptsToOnline",
                        _, _, _, _)).Times(0);
  EXPECT_CALL(metrics_,
              SendToUMA("Network.Shill.Unknown.PortalAttempts",
                        kPortalAttempts,
                        Metrics::kMetricPortalAttemptsMin,
                        Metrics::kMetricPortalAttemptsMax,
                        Metrics::kMetricPortalAttemptsNumBuckets));
  EXPECT_CALL(*connection_.get(), is_default())
      .WillOnce(Return(false));
  PortalDetectorCallback(PortalDetector::Result(
      PortalDetector::kPhaseConnection,
      PortalDetector::kStatusFailure,
      kPortalAttempts,
      true));
  Mock::VerifyAndClearExpectations(&metrics_);

  EXPECT_CALL(*service_.get(), SetState(Service::kStateOnline));
  EXPECT_CALL(metrics_,
              SendEnumToUMA("Network.Shill.Unknown.PortalResult",
                            Metrics::kPortalResultSuccess,
                            Metrics::kPortalResultMax));
  EXPECT_CALL(metrics_,
              SendToUMA("Network.Shill.Unknown.PortalAttemptsToOnline",
                        kPortalAttempts * 2,
                        Metrics::kMetricPortalAttemptsToOnlineMin,
                        Metrics::kMetricPortalAttemptsToOnlineMax,
                        Metrics::kMetricPortalAttemptsToOnlineNumBuckets));
  EXPECT_CALL(metrics_,
              SendToUMA("Network.Shill.Unknown.PortalAttempts",
                        _, _, _, _)).Times(0);
  PortalDetectorCallback(PortalDetector::Result(
      PortalDetector::kPhaseContent,
      PortalDetector::kStatusSuccess,
      kPortalAttempts,
      true));
}

TEST_F(DevicePortalDetectionTest, RequestPortalDetection) {
  EXPECT_CALL(*service_.get(), state())
      .WillOnce(Return(Service::kStateOnline))
      .WillRepeatedly(Return(Service::kStatePortal));
  EXPECT_FALSE(RequestPortalDetection());

  EXPECT_CALL(*connection_.get(), is_default())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(RequestPortalDetection());

  EXPECT_CALL(*portal_detector_, IsInProgress())
      .WillOnce(Return(true));
  // Portal detection already running.
  EXPECT_TRUE(RequestPortalDetection());

  // Make sure our running mock portal detector was not replaced.
  ExpectPortalDetectorIsMock();

  // Throw away our pre-fabricated portal detector, and have the device create
  // a new one.
  StopPortalDetection();
  EXPECT_CALL(manager_, IsPortalDetectionEnabled(device_->technology()))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*service_.get(), HasProxyConfig())
      .WillRepeatedly(Return(false));
  const string kPortalCheckURL("http://portal");
  EXPECT_CALL(manager_, GetPortalCheckURL())
      .WillOnce(ReturnRef(kPortalCheckURL));
  const string kInterfaceName("int0");
  EXPECT_CALL(*connection_.get(), interface_name())
      .WillRepeatedly(ReturnRef(kInterfaceName));
  const vector<string> kDNSServers;
  EXPECT_CALL(*connection_.get(), dns_servers())
      .WillRepeatedly(ReturnRef(kDNSServers));
  EXPECT_TRUE(RequestPortalDetection());
}

TEST_F(DevicePortalDetectionTest, NotConnected) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillOnce(Return(false));
  SetServiceConnectedState(Service::kStatePortal);
  // We don't check for the portal detector to be reset here, because
  // it would have been reset as a part of disconnection.
}

TEST_F(DevicePortalDetectionTest, NotPortal) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillOnce(Return(true));
  EXPECT_CALL(*service_.get(), SetState(Service::kStateOnline));
  SetServiceConnectedState(Service::kStateOnline);
  ExpectPortalDetectorReset();
}

TEST_F(DevicePortalDetectionTest, NotDefault) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillOnce(Return(true));
  EXPECT_CALL(*connection_.get(), is_default())
      .WillOnce(Return(false));
  EXPECT_CALL(*service_.get(), SetState(Service::kStatePortal));
  SetServiceConnectedState(Service::kStatePortal);
  ExpectPortalDetectorReset();
}

TEST_F(DevicePortalDetectionTest, PortalIntervalIsZero) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillOnce(Return(true));
  EXPECT_CALL(*connection_.get(), is_default())
      .WillOnce(Return(true));
  EXPECT_CALL(manager_, GetPortalCheckInterval())
      .WillOnce(Return(0));
  EXPECT_CALL(*service_.get(), SetState(Service::kStatePortal));
  SetServiceConnectedState(Service::kStatePortal);
  ExpectPortalDetectorReset();
}

TEST_F(DevicePortalDetectionTest, RestartPortalDetection) {
  EXPECT_CALL(*service_.get(), IsConnected())
      .WillOnce(Return(true));
  EXPECT_CALL(*connection_.get(), is_default())
      .WillOnce(Return(true));
  const int kPortalDetectionInterval = 10;
  EXPECT_CALL(manager_, GetPortalCheckInterval())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(kPortalDetectionInterval));
  const string kPortalCheckURL("http://portal");
  EXPECT_CALL(manager_, GetPortalCheckURL())
      .WillOnce(ReturnRef(kPortalCheckURL));
  EXPECT_CALL(*portal_detector_, StartAfterDelay(kPortalCheckURL,
                                                 kPortalDetectionInterval))
      .WillOnce(Return(true));
  EXPECT_CALL(*service_.get(), SetState(Service::kStatePortal));
  SetServiceConnectedState(Service::kStatePortal);
  ExpectPortalDetectorSet();
}

}  // namespace shill
