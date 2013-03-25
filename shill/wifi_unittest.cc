// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi.h"

#include <netinet/ether.h>
#include <linux/if.h>
#include <sys/socket.h>
#include <linux/netlink.h>  // Needs typedefs from sys/socket.h.

#include <map>
#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/stringprintf.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/dbus_adaptor.h"
#include "shill/event_dispatcher.h"
#include "shill/geolocation_info.h"
#include "shill/ieee80211.h"
#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/mock_dbus_manager.h"
#include "shill/mock_device.h"
#include "shill/mock_device_info.h"
#include "shill/mock_dhcp_config.h"
#include "shill/mock_dhcp_provider.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_link_monitor.h"
#include "shill/mock_log.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_profile.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/mock_store.h"
#include "shill/mock_supplicant_bss_proxy.h"
#include "shill/mock_supplicant_interface_proxy.h"
#include "shill/mock_supplicant_network_proxy.h"
#include "shill/mock_supplicant_process_proxy.h"
#include "shill/mock_time.h"
#include "shill/mock_wifi_provider.h"
#include "shill/mock_wifi_service.h"
#include "shill/nice_mock_control.h"
#include "shill/property_store_unittest.h"
#include "shill/proxy_factory.h"
#include "shill/technology.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi_service.h"
#include "shill/wpa_supplicant.h"


using base::FilePath;
using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::DefaultValue;
using ::testing::DoAll;
using ::testing::EndsWith;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::Test;
using ::testing::Throw;
using ::testing::Values;

namespace shill {

class WiFiPropertyTest : public PropertyStoreTest {
 public:
  WiFiPropertyTest()
      : device_(new WiFi(control_interface(),
                         NULL, NULL, manager(), "wifi", "", 0)) {
  }
  virtual ~WiFiPropertyTest() {}

 protected:
  WiFiRefPtr device_;
};

TEST_F(WiFiPropertyTest, Contains) {
  EXPECT_TRUE(device_->store().Contains(flimflam::kNameProperty));
  EXPECT_FALSE(device_->store().Contains(""));
}

TEST_F(WiFiPropertyTest, SetProperty) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(
        device_->mutable_store(),
        flimflam::kBgscanSignalThresholdProperty,
        PropertyStoreTest::kInt32V,
        &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                         flimflam::kScanIntervalProperty,
                                         PropertyStoreTest::kUint16V,
                                         &error));
  }
  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                          flimflam::kScanningProperty,
                                          PropertyStoreTest::kBoolV,
                                          &error));
    EXPECT_EQ(invalid_args(), error.name());
  }

  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(
        device_->mutable_store(),
        flimflam::kBgscanMethodProperty,
        DBusAdaptor::StringToVariant(
            WPASupplicant::kNetworkBgscanMethodSimple),
        &error));
  }

  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(
        device_->mutable_store(),
        flimflam::kBgscanMethodProperty,
        DBusAdaptor::StringToVariant("not a real scan method"),
        &error));
  }
}

TEST_F(WiFiPropertyTest, BgscanMethodProperty) {
  EXPECT_NE(WPASupplicant::kNetworkBgscanMethodLearn,
            WiFi::kDefaultBgscanMethod);
  EXPECT_TRUE(device_->bgscan_method_.empty());

  string method;
  Error unused_error;
  EXPECT_TRUE(device_->store().GetStringProperty(
      flimflam::kBgscanMethodProperty, &method, &unused_error));
  EXPECT_EQ(WiFi::kDefaultBgscanMethod, method);
  EXPECT_EQ(WPASupplicant::kNetworkBgscanMethodSimple, method);

  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::SetProperty(
      device_->mutable_store(),
      flimflam::kBgscanMethodProperty,
      DBusAdaptor::StringToVariant(
          WPASupplicant::kNetworkBgscanMethodLearn),
      &error));
  EXPECT_EQ(WPASupplicant::kNetworkBgscanMethodLearn, device_->bgscan_method_);
  EXPECT_TRUE(device_->store().GetStringProperty(
      flimflam::kBgscanMethodProperty, &method, &unused_error));
  EXPECT_EQ(WPASupplicant::kNetworkBgscanMethodLearn, method);

  EXPECT_TRUE(DBusAdaptor::ClearProperty(
      device_->mutable_store(), flimflam::kBgscanMethodProperty, &error));
  EXPECT_TRUE(device_->store().GetStringProperty(
      flimflam::kBgscanMethodProperty, &method, &unused_error));
  EXPECT_EQ(WiFi::kDefaultBgscanMethod, method);
  EXPECT_TRUE(device_->bgscan_method_.empty());
}


MATCHER_P(EndpointMatch, endpoint, "") {
  return
      arg->ssid() == endpoint->ssid() &&
      arg->network_mode() == endpoint->network_mode() &&
      arg->security_mode() == endpoint->security_mode();
}

class WiFiObjectTest : public ::testing::TestWithParam<string> {
 public:
  WiFiObjectTest(EventDispatcher *dispatcher)
      : event_dispatcher_(dispatcher),
        metrics_(NULL),
        manager_(&control_interface_, NULL, &metrics_, &glib_),
        device_info_(&control_interface_, dispatcher, &metrics_, &manager_),
        wifi_(new WiFi(&control_interface_,
                       dispatcher,
                       &metrics_,
                       &manager_,
                       kDeviceName,
                       kDeviceAddress,
                       0)),
        bss_counter_(0),
        supplicant_process_proxy_(new NiceMock<MockSupplicantProcessProxy>()),
        supplicant_bss_proxy_(
            new NiceMock<MockSupplicantBSSProxy>()),
        dhcp_config_(new MockDHCPConfig(&control_interface_,
                                        kDeviceName)),
        dbus_manager_(new NiceMock<MockDBusManager>()),
        supplicant_interface_proxy_(
            new NiceMock<MockSupplicantInterfaceProxy>(wifi_)),
        proxy_factory_(this) {
    ::testing::DefaultValue< ::DBus::Path>::Set("/default/path");

    ON_CALL(dhcp_provider_, CreateConfig(_, _, _, _)).
        WillByDefault(Return(dhcp_config_));
    ON_CALL(*dhcp_config_.get(), RequestIP()).
        WillByDefault(Return(true));
    ON_CALL(proxy_factory_, CreateSupplicantNetworkProxy(_, _)).
        WillByDefault(InvokeWithoutArgs(
            this, &WiFiObjectTest::CreateSupplicantNetworkProxy));

    manager_.dbus_manager_.reset(dbus_manager_);  // Transfers ownership.

    wifi_->provider_ = &wifi_provider_;
    wifi_->time_ = &time_;
  }

  virtual void SetUp() {
    // EnableScopes... so that we can EXPECT_CALL for scoped log messages.
    ScopeLogger::GetInstance()->EnableScopesByName("wifi");
    ScopeLogger::GetInstance()->set_verbose_level(3);
    wifi_->proxy_factory_ = &proxy_factory_;
    static_cast<Device *>(wifi_)->rtnl_handler_ = &rtnl_handler_;
    wifi_->set_dhcp_provider(&dhcp_provider_);
    ON_CALL(manager_, device_info()).WillByDefault(Return(&device_info_));
    EXPECT_CALL(manager_, UpdateEnabledTechnologies()).Times(AnyNumber());
    EXPECT_CALL(*supplicant_bss_proxy_, Die()).Times(AnyNumber());
  }

  virtual void TearDown() {
    EXPECT_CALL(*wifi_provider(), OnEndpointRemoved(_))
       .WillRepeatedly(Return(reinterpret_cast<WiFiService *>(NULL)));
    wifi_->SelectService(NULL);
    if (supplicant_bss_proxy_.get()) {
      EXPECT_CALL(*supplicant_bss_proxy_, Die());
    }
    wifi_->proxy_factory_ = NULL;
    // must Stop WiFi instance, to clear its list of services.
    // otherwise, the WiFi instance will not be deleted. (because
    // services reference a WiFi instance, creating a cycle.)
    wifi_->Stop(NULL, ResultCallback());
    wifi_->set_dhcp_provider(NULL);
    // Reset scope logging, to avoid interfering with other tests.
    ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
    ScopeLogger::GetInstance()->set_verbose_level(0);
  }

  // Needs to be public since it is called via Invoke().
  void StopWiFi() {
    wifi_->SetEnabled(false);  // Stop(NULL, ResultCallback());
  }

  // Needs to be public since it is called via Invoke().
  void ThrowDBusError() {
    throw DBus::Error("SomeDBusType", "A handy message");
  }

 protected:
  typedef scoped_refptr<MockWiFiService> MockWiFiServiceRefPtr;

  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(WiFiObjectTest *test);

    virtual SupplicantProcessProxyInterface *CreateSupplicantProcessProxy(
        const char */*dbus_path*/, const char */*dbus_addr*/) {
      return test_->supplicant_process_proxy_.release();
    }

    virtual SupplicantInterfaceProxyInterface *CreateSupplicantInterfaceProxy(
        SupplicantEventDelegateInterface */*delegate*/,
        const DBus::Path &/*object_path*/,
        const char */*dbus_addr*/) {
      return test_->supplicant_interface_proxy_.release();
    }

    MOCK_METHOD3(CreateSupplicantBSSProxy,
                 SupplicantBSSProxyInterface *(
                     WiFiEndpoint *wifi_endpoint,
                     const DBus::Path &object_path,
                     const char *dbus_addr));

    MOCK_METHOD2(CreateSupplicantNetworkProxy,
                 SupplicantNetworkProxyInterface *(
                     const DBus::Path &object_path,
                     const char *dbus_addr));

   private:
    SupplicantBSSProxyInterface *CreateSupplicantBSSProxyInternal(
        WiFiEndpoint */*wifi_endpoint*/,
        const DBus::Path &/*object_path*/,
        const char */*dbus_addr*/) {
      return test_->supplicant_bss_proxy_.release();
    }

    WiFiObjectTest *test_;
  };

  // Simulate the course of events when the last enpoint of a service is
  // removed.
  class EndpointRemovalHandler {
    public:
      EndpointRemovalHandler(WiFiRefPtr wifi, const WiFiServiceRefPtr &service)
          : wifi_(wifi), service_(service) {}
      virtual ~EndpointRemovalHandler() {}

      WiFiServiceRefPtr OnEndpointRemoved(
          const WiFiEndpointConstRefPtr &endpoint) {
        wifi_->DisassociateFromService(service_);
        return service_;
      }

   private:
    WiFiRefPtr wifi_;
    WiFiServiceRefPtr service_;
  };

  EndpointRemovalHandler *MakeEndpointRemovalHandler(
      WiFiServiceRefPtr &service) {
    return new EndpointRemovalHandler(wifi_, service);
  }
  void CancelScanTimer() {
    wifi_->scan_timer_callback_.Cancel();
  }
  // This function creates a new endpoint with a mode set to |mode|.  We
  // synthesize new |path| and |bssid| values, since we don't really care
  // what they are for unit tests.  If "use_ssid" is true, we used the
  // passed-in ssid, otherwise we create a synthesized value for it as well.
  WiFiEndpointRefPtr MakeNewEndpoint(const char *mode,
                                     bool use_ssid,
                                     string *ssid,
                                     string *path,
                                     string *bssid) {
    bss_counter_++;
    if (!use_ssid) {
      *ssid = base::StringPrintf("ssid%d", bss_counter_);
    }
    *path = base::StringPrintf("/interface/bss%d", bss_counter_);
    *bssid = base::StringPrintf("00:00:00:00:00:%02x", bss_counter_);
    WiFiEndpointRefPtr endpoint = MakeEndpointWithMode(*ssid, *bssid, mode);
    EXPECT_CALL(wifi_provider_,
                OnEndpointAdded(EndpointMatch(endpoint))).Times(1);
    return endpoint;
  }
  WiFiEndpointRefPtr MakeEndpoint(const string &ssid, const string &bssid) {
    return MakeEndpointWithMode(ssid, bssid, kNetworkModeInfrastructure);
  }
  WiFiEndpointRefPtr MakeEndpointWithMode(
      const string &ssid, const string &bssid, const string &mode) {
    return WiFiEndpoint::MakeOpenEndpoint(
        &proxy_factory_, NULL, ssid, bssid, mode, 0, 0);
  }
  MockWiFiServiceRefPtr MakeMockServiceWithSSID(
      vector<uint8_t> ssid, const std::string &security) {
    return new NiceMock<MockWiFiService>(
        &control_interface_,
        event_dispatcher_,
        &metrics_,
        &manager_,
        &wifi_provider_,
        ssid,
        flimflam::kModeManaged,
        security,
        false);
  }
  MockWiFiServiceRefPtr MakeMockService(const std::string &security) {
    return MakeMockServiceWithSSID(vector<uint8_t>(1, 'a'), security);
  }
  ::DBus::Path MakeNewEndpointAndService(int16_t signal_strength,
                                         uint16 frequency,
                                         const char *mode,
                                         WiFiEndpointRefPtr *endpoint_ptr,
                                         MockWiFiServiceRefPtr *service_ptr) {
    string ssid;
    ::DBus::Path path;
    string bssid;
    WiFiEndpointRefPtr endpoint =
        MakeNewEndpoint(mode, false, &ssid, &path, &bssid);
    MockWiFiServiceRefPtr service =
        MakeMockServiceWithSSID(endpoint->ssid(), endpoint->security_mode());
    EXPECT_CALL(wifi_provider_, FindServiceForEndpoint(EndpointMatch(endpoint)))
        .WillRepeatedly(Return(service));
    ON_CALL(*service, GetEndpointCount()).WillByDefault(Return(1));
    ReportBSS(path, ssid, bssid, signal_strength, frequency, mode);
    if (service_ptr) {
      *service_ptr = service;
    }
    if (endpoint_ptr) {
      *endpoint_ptr = endpoint;
    }
    return path;
  }
  ::DBus::Path AddEndpointToService(
      WiFiServiceRefPtr service,
      int16_t signal_strength,
      uint16 frequency,
      const char *mode,
      WiFiEndpointRefPtr *endpoint_ptr) {
    string ssid(service->ssid().begin(), service->ssid().end());
    ::DBus::Path path;
    string bssid;
    WiFiEndpointRefPtr endpoint =
        MakeNewEndpoint(mode, true, &ssid, &path, &bssid);
    EXPECT_CALL(wifi_provider_, FindServiceForEndpoint(EndpointMatch(endpoint)))
        .WillRepeatedly(Return(service));
    ReportBSS(path, ssid, bssid, signal_strength, frequency, mode);
    if (endpoint_ptr) {
      *endpoint_ptr = endpoint;
    }
    return path;
  }
  void InitiateConnect(WiFiServiceRefPtr service) {
    map<string, ::DBus::Variant> params;
    wifi_->ConnectTo(service, params);
  }
  void InitiateDisconnect(WiFiServiceRefPtr service) {
    wifi_->DisconnectFrom(service);
  }
  MockWiFiServiceRefPtr SetupConnectingService(
      const DBus::Path &network_path,
      WiFiEndpointRefPtr *endpoint_ptr,
      ::DBus::Path *bss_path_ptr) {
    if (!network_path.empty()) {
      EXPECT_CALL(*GetSupplicantInterfaceProxy(), AddNetwork(_))
          .WillOnce(Return(network_path));
      EXPECT_CALL(*GetSupplicantInterfaceProxy(), SelectNetwork(network_path));
    }

    MockWiFiServiceRefPtr service;
    WiFiEndpointRefPtr endpoint;
    ::DBus::Path bss_path(MakeNewEndpointAndService(
        0, 0, kNetworkModeAdHoc, &endpoint, &service));
    EXPECT_CALL(*service, SetState(Service::kStateAssociating));
    InitiateConnect(service);
    Mock::VerifyAndClearExpectations(service);
    EXPECT_FALSE(GetPendingTimeout().IsCancelled());
    if (endpoint_ptr) {
      *endpoint_ptr = endpoint;
    }
    if (bss_path_ptr) {
      *bss_path_ptr = bss_path;
    }
    return service;
  }

  MockWiFiServiceRefPtr SetupConnectedService(
      const DBus::Path &network_path,
      WiFiEndpointRefPtr *endpoint_ptr,
      ::DBus::Path *bss_path_ptr) {
    WiFiEndpointRefPtr endpoint;
    ::DBus::Path bss_path;
    MockWiFiServiceRefPtr service =
        SetupConnectingService(network_path, &endpoint, &bss_path);
    if (endpoint_ptr) {
      *endpoint_ptr = endpoint;
    }
    if (bss_path_ptr) {
      *bss_path_ptr = bss_path;
    }
    EXPECT_CALL(*service, NotifyCurrentEndpoint(EndpointMatch(endpoint)));
    ReportCurrentBSSChanged(bss_path);
    EXPECT_TRUE(GetPendingTimeout().IsCancelled());
    Mock::VerifyAndClearExpectations(service);

    EXPECT_CALL(*service, SetState(Service::kStateConfiguring));
    EXPECT_CALL(*dhcp_provider(), CreateConfig(_, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(*dhcp_config_.get(), RequestIP()).Times(AnyNumber());
    ReportStateChanged(WPASupplicant::kInterfaceStateCompleted);
    Mock::VerifyAndClearExpectations(service);

    EXPECT_EQ(service, GetCurrentService());
    return service;
  }
  void FireScanTimer() {
    wifi_->ScanTimerHandler();
  }
  void TriggerScan() {
    wifi_->Scan(NULL);
  }
  const WiFiServiceRefPtr &GetCurrentService() {
    return wifi_->current_service_;
  }
  void SetCurrentService(const WiFiServiceRefPtr &service) {
    wifi_->current_service_ = service;
  }
  const WiFi::EndpointMap &GetEndpointMap() {
    return wifi_->endpoint_by_rpcid_;
  }
  const WiFiServiceRefPtr &GetPendingService() {
    return wifi_->pending_service_;
  }
  const base::CancelableClosure &GetPendingTimeout() {
    return wifi_->pending_timeout_callback_;
  }
  const base::CancelableClosure &GetReconnectTimeoutCallback() {
    return wifi_->reconnect_timeout_callback_;
  }
  int GetReconnectTimeoutSeconds() {
    return WiFi::kReconnectTimeoutSeconds;
  }
  const base::CancelableClosure &GetScanTimer() {
    return wifi_->scan_timer_callback_;
  }
  // note: the tests need the proxies referenced by WiFi (not the
  // proxies instantiated by WiFiObjectTest), to ensure that WiFi
  // sets up its proxies correctly.
  SupplicantProcessProxyInterface *GetSupplicantProcessProxy() {
    return wifi_->supplicant_process_proxy_.get();
  }
  MockSupplicantInterfaceProxy *GetSupplicantInterfaceProxyFromWiFi() {
    return dynamic_cast<MockSupplicantInterfaceProxy *>(
        wifi_->supplicant_interface_proxy_.get());
  }
  // This function returns the supplicant interface proxy whether
  // or not we have passed the instantiated object to the WiFi instance
  // from WiFiObjectTest, so tests don't need to worry about when they
  // set expectations relative to StartWiFi().
  MockSupplicantInterfaceProxy *GetSupplicantInterfaceProxy() {
    MockSupplicantInterfaceProxy *proxy = GetSupplicantInterfaceProxyFromWiFi();
    return proxy ? proxy : supplicant_interface_proxy_.get();
  }
  MockSupplicantNetworkProxy *CreateSupplicantNetworkProxy() {
    return new NiceMock<MockSupplicantNetworkProxy>();
  }
  const string &GetSupplicantState() {
    return wifi_->supplicant_state_;
  }
  void ClearCachedCredentials(const WiFiService *service) {
    return wifi_->ClearCachedCredentials(service);
  }
  void NotifyEndpointChanged(const WiFiEndpointConstRefPtr &endpoint) {
    wifi_->NotifyEndpointChanged(endpoint);
  }
  bool RemoveNetwork(const ::DBus::Path &network) {
    return wifi_->RemoveNetwork(network);
  }
  void RemoveBSS(const ::DBus::Path &bss_path);
  void ReportBSS(const ::DBus::Path &bss_path,
                 const string &ssid,
                 const string &bssid,
                 int16_t signal_strength,
                 uint16 frequency,
                 const char *mode);
  void ReportIPConfigComplete() {
    wifi_->OnIPConfigUpdated(dhcp_config_, true);
  }
  void ReportLinkUp() {
    wifi_->LinkEvent(IFF_LOWER_UP, IFF_LOWER_UP);
  }
  void ReportScanDone() {
    wifi_->ScanDoneTask();
  }
  void ReportCurrentBSSChanged(const string &new_bss) {
    wifi_->CurrentBSSChanged(new_bss);
  }
  void ReportStateChanged(const string &new_state) {
    wifi_->StateChanged(new_state);
  }
  void ReportWiFiDebugScopeChanged(bool enabled) {
    wifi_->OnWiFiDebugScopeChanged(enabled);
  }
  void SetPendingService(const WiFiServiceRefPtr &service) {
    wifi_->pending_service_ = service;
  }
  void SetScanInterval(uint16_t interval_seconds) {
    wifi_->SetScanInterval(interval_seconds, NULL);
  }
  uint16_t GetScanInterval() {
    return wifi_->GetScanInterval(NULL);
  }
  void StartWiFi(bool supplicant_present) {
    wifi_->supplicant_present_ = supplicant_present;
    wifi_->SetEnabled(true);  // Start(NULL, ResultCallback());
  }
  void StartWiFi() {
    StartWiFi(true);
  }
  void OnAfterResume() {
    wifi_->OnAfterResume();
  }
  void OnBeforeSuspend() {
    wifi_->OnBeforeSuspend();
  }
  void OnSupplicantAppear() {
    wifi_->OnSupplicantAppear(":1.7");
    EXPECT_TRUE(wifi_->supplicant_present_);
  }
  void OnSupplicantVanish() {
    wifi_->OnSupplicantVanish();
    EXPECT_FALSE(wifi_->supplicant_present_);
  }
  bool GetSupplicantPresent() {
    return wifi_->supplicant_present_;
  }
  bool SetBgscanMethod(const string &method) {
    ::DBus::Error error;
    return DBusAdaptor::SetProperty(
        wifi_->mutable_store(),
        flimflam::kBgscanMethodProperty,
        DBusAdaptor::StringToVariant(method),
        &error);
  }

  void AppendBgscan(WiFiService *service,
                    std::map<std::string, DBus::Variant> *service_params) {
    wifi_->AppendBgscan(service, service_params);
  }

  void ReportCertification(const map<string, ::DBus::Variant> &properties) {
    wifi_->CertificationTask(properties);
  }

  void ReportEAPEvent(const string &status, const string &parameter) {
    wifi_->EAPEventTask(status, parameter);
  }

  void RestartFastScanAttempts() {
    wifi_->RestartFastScanAttempts();
  }

  void StartReconnectTimer() {
    wifi_->StartReconnectTimer();
  }

  void StopReconnectTimer() {
    wifi_->StopReconnectTimer();
  }

  void SetLinkMonitor(LinkMonitor *link_monitor) {
    wifi_->set_link_monitor(link_monitor);
  }

  bool SuspectCredentials(const WiFiService &service,
                          Service::ConnectFailure *failure) {
    return wifi_->SuspectCredentials(service, failure);
  }

  void OnLinkMonitorFailure() {
    wifi_->OnLinkMonitorFailure();
  }

  NiceMockControl *control_interface() {
    return &control_interface_;
  }

  MockMetrics *metrics() {
    return &metrics_;
  }

  MockManager *manager() {
    return &manager_;
  }

  MockDeviceInfo *device_info() {
    return &device_info_;
  }

  MockDHCPProvider *dhcp_provider() {
    return &dhcp_provider_;
  }

  const WiFiConstRefPtr wifi() const {
    return wifi_;
  }

  TestProxyFactory *proxy_factory() {
    return &proxy_factory_;
  }

  MockWiFiProvider *wifi_provider() {
    return &wifi_provider_;
  }

  EventDispatcher *event_dispatcher_;
  NiceMock<MockRTNLHandler> rtnl_handler_;
  MockTime time_;

 private:
  NiceMockControl control_interface_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  WiFiRefPtr wifi_;
  NiceMock<MockWiFiProvider> wifi_provider_;
  int bss_counter_;

  // protected fields interspersed between private fields, due to
  // initialization order
 protected:
  static const char kDeviceName[];
  static const char kDeviceAddress[];
  static const char kNetworkModeAdHoc[];
  static const char kNetworkModeInfrastructure[];
  static const char kBSSName[];
  static const char kSSIDName[];

  scoped_ptr<MockSupplicantProcessProxy> supplicant_process_proxy_;
  scoped_ptr<MockSupplicantBSSProxy> supplicant_bss_proxy_;
  MockDHCPProvider dhcp_provider_;
  scoped_refptr<MockDHCPConfig> dhcp_config_;
  NiceMock<MockDBusManager> *dbus_manager_;

 private:
  scoped_ptr<MockSupplicantInterfaceProxy> supplicant_interface_proxy_;
  NiceMock<TestProxyFactory> proxy_factory_;
};

const char WiFiObjectTest::kDeviceName[] = "wlan0";
const char WiFiObjectTest::kDeviceAddress[] = "000102030405";
const char WiFiObjectTest::kNetworkModeAdHoc[] = "ad-hoc";
const char WiFiObjectTest::kNetworkModeInfrastructure[] = "infrastructure";
const char WiFiObjectTest::kBSSName[] = "bss0";
const char WiFiObjectTest::kSSIDName[] = "ssid0";

void WiFiObjectTest::RemoveBSS(const ::DBus::Path &bss_path) {
  wifi_->BSSRemovedTask(bss_path);
}

void WiFiObjectTest::ReportBSS(const ::DBus::Path &bss_path,
                             const string &ssid,
                             const string &bssid,
                             int16_t signal_strength,
                             uint16 frequency,
                             const char *mode) {
  map<string, ::DBus::Variant> bss_properties;

  {
    DBus::MessageIter writer(bss_properties["SSID"].writer());
    writer << vector<uint8_t>(ssid.begin(), ssid.end());
  }
  {
    string bssid_nosep;
    vector<uint8_t> bssid_bytes;
    RemoveChars(bssid, ":", &bssid_nosep);
    base::HexStringToBytes(bssid_nosep, &bssid_bytes);

    DBus::MessageIter writer(bss_properties["BSSID"].writer());
    writer << bssid_bytes;
  }
  bss_properties[WPASupplicant::kBSSPropertySignal].writer().
      append_int16(signal_strength);
  bss_properties[WPASupplicant::kBSSPropertyFrequency].writer().
      append_uint16(frequency);
  bss_properties[WPASupplicant::kBSSPropertyMode].writer().append_string(mode);
  wifi_->BSSAddedTask(bss_path, bss_properties);
}

WiFiObjectTest::TestProxyFactory::TestProxyFactory(WiFiObjectTest *test)
    : test_(test) {
  EXPECT_CALL(*this, CreateSupplicantBSSProxy(_, _, _)).Times(AnyNumber());
  ON_CALL(*this, CreateSupplicantBSSProxy(_, _, _))
      .WillByDefault(
          Invoke(this, (&TestProxyFactory::CreateSupplicantBSSProxyInternal)));
}

// Most of our tests involve using a real EventDispatcher object.
class WiFiMainTest : public WiFiObjectTest {
 public:
  WiFiMainTest() : WiFiObjectTest(&dispatcher_) {}

 protected:
  EventDispatcher dispatcher_;
};

TEST_F(WiFiMainTest, ProxiesSetUpDuringStart) {
  EXPECT_TRUE(GetSupplicantProcessProxy() == NULL);
  EXPECT_TRUE(GetSupplicantInterfaceProxyFromWiFi() == NULL);

  StartWiFi();
  EXPECT_FALSE(GetSupplicantProcessProxy() == NULL);
  EXPECT_FALSE(GetSupplicantInterfaceProxyFromWiFi() == NULL);
}

TEST_F(WiFiMainTest, SupplicantPresent) {
  EXPECT_FALSE(GetSupplicantPresent());
}

TEST_F(WiFiMainTest, OnSupplicantAppearStarted) {
  EXPECT_TRUE(GetSupplicantProcessProxy() == NULL);

  EXPECT_CALL(*dbus_manager_, WatchName(WPASupplicant::kDBusAddr, _, _));
  StartWiFi(false);  // No supplicant present.
  EXPECT_TRUE(GetSupplicantProcessProxy() == NULL);

  OnSupplicantAppear();
  EXPECT_FALSE(GetSupplicantProcessProxy() == NULL);

  // If supplicant reappears while the device is started, the device should be
  // restarted.
  EXPECT_CALL(*manager(), DeregisterDevice(_));
  EXPECT_CALL(*manager(), RegisterDevice(_));
  OnSupplicantAppear();
}

TEST_F(WiFiMainTest, OnSupplicantAppearStopped) {
  EXPECT_TRUE(GetSupplicantProcessProxy() == NULL);

  OnSupplicantAppear();
  EXPECT_TRUE(GetSupplicantProcessProxy() == NULL);

  // If supplicant reappears while the device is stopped, the device should not
  // be restarted.
  EXPECT_CALL(*manager(), DeregisterDevice(_)).Times(0);
  OnSupplicantAppear();
}

TEST_F(WiFiMainTest, OnSupplicantVanishStarted) {
  EXPECT_TRUE(GetSupplicantProcessProxy() == NULL);

  StartWiFi();
  EXPECT_FALSE(GetSupplicantProcessProxy() == NULL);
  EXPECT_TRUE(GetSupplicantPresent());

  EXPECT_CALL(*manager(), DeregisterDevice(_));
  EXPECT_CALL(*manager(), RegisterDevice(_));
  OnSupplicantVanish();
}

TEST_F(WiFiMainTest, OnSupplicantVanishStopped) {
  OnSupplicantAppear();
  EXPECT_TRUE(GetSupplicantPresent());
  EXPECT_CALL(*manager(), DeregisterDevice(_)).Times(0);
  OnSupplicantVanish();
}

TEST_F(WiFiMainTest, OnSupplicantVanishedWhileConnected) {
  StartWiFi();
  WiFiEndpointRefPtr endpoint;
  WiFiServiceRefPtr service(
      SetupConnectedService(DBus::Path(), &endpoint, NULL));
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                       EndsWith("silently resetting current_service_.")));
  EXPECT_CALL(*manager(), DeregisterDevice(_))
      .WillOnce(InvokeWithoutArgs(this, &WiFiObjectTest::StopWiFi));
  scoped_ptr<EndpointRemovalHandler> handler(
      MakeEndpointRemovalHandler(service));
  EXPECT_CALL(*wifi_provider(), OnEndpointRemoved(EndpointMatch(endpoint)))
      .WillOnce(Invoke(handler.get(),
                &EndpointRemovalHandler::OnEndpointRemoved));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect()).Times(0);
  EXPECT_CALL(*manager(), RegisterDevice(_));
  OnSupplicantVanish();
  EXPECT_TRUE(GetCurrentService() == NULL);
}

TEST_F(WiFiMainTest, CleanStart) {
  EXPECT_CALL(*supplicant_process_proxy_, CreateInterface(_));
  EXPECT_CALL(*supplicant_process_proxy_, GetInterface(_))
      .Times(AnyNumber())
      .WillRepeatedly(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.InterfaceUnknown",
              "test threw fi.w1.wpa_supplicant1.InterfaceUnknown")));
  EXPECT_TRUE(GetScanTimer().IsCancelled());
  StartWiFi();
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
  dispatcher_.DispatchPendingEvents();
  EXPECT_FALSE(GetScanTimer().IsCancelled());
}

TEST_F(WiFiMainTest, ClearCachedCredentials) {
  StartWiFi();
  DBus::Path network = "/test/path";
  WiFiServiceRefPtr service(SetupConnectedService(network, NULL, NULL));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(network));
  ClearCachedCredentials(service);
}

TEST_F(WiFiMainTest, NotifyEndpointChanged) {
  WiFiEndpointRefPtr endpoint =
      MakeEndpointWithMode("ssid", "00:00:00:00:00:00", kNetworkModeAdHoc);
  EXPECT_CALL(*wifi_provider(), OnEndpointUpdated(EndpointMatch(endpoint)));
  NotifyEndpointChanged(endpoint);
}

TEST_F(WiFiMainTest, RemoveNetwork) {
  DBus::Path network = "/test/path";
  StartWiFi();
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(network));
  EXPECT_TRUE(RemoveNetwork(network));
}

TEST_F(WiFiMainTest, RemoveNetworkWhenSupplicantReturnsNetworkUnknown) {
  DBus::Path network = "/test/path";
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(network))
      .WillRepeatedly(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.NetworkUnknown",
              "test threw fi.w1.wpa_supplicant1.NetworkUnknown")));
  StartWiFi();
  EXPECT_TRUE(RemoveNetwork(network));
}

TEST_F(WiFiMainTest, UseArpGateway) {
  EXPECT_CALL(dhcp_provider_, CreateConfig(kDeviceName, _, _, true))
      .WillOnce(Return(dhcp_config_));
  const_cast<WiFi *>(wifi().get())->AcquireIPConfig();
}

TEST_F(WiFiMainTest, RemoveNetworkWhenSupplicantReturnsInvalidArgs) {
  DBus::Path network = "/test/path";
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(network))
      .WillRepeatedly(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.InvalidArgs",
              "test threw fi.w1.wpa_supplicant1.InvalidArgs")));
  StartWiFi();
  EXPECT_FALSE(RemoveNetwork(network));
}

TEST_F(WiFiMainTest, RemoveNetworkWhenSupplicantReturnsUnknownError) {
  DBus::Path network = "/test/path";
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(network))
      .WillRepeatedly(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.UnknownError",
              "test threw fi.w1.wpa_supplicant1.UnknownError")));
  StartWiFi();
  EXPECT_FALSE(RemoveNetwork(network));
}

TEST_F(WiFiMainTest, Restart) {
  EXPECT_CALL(*supplicant_process_proxy_, CreateInterface(_))
      .Times(AnyNumber())
      .WillRepeatedly(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.InterfaceExists",
              "test threw fi.w1.wpa_supplicant1.InterfaceExists")));
  EXPECT_CALL(*supplicant_process_proxy_, GetInterface(_));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, StartClearsState) {
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveAllNetworks());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), FlushBSS(_));
  StartWiFi();
}

TEST_F(WiFiMainTest, NoScansWhileConnecting) {
  StartWiFi();
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(1);
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurityNone);
  SetPendingService(service);
  // If we're connecting, we ignore scan requests to stay on channel.
  EXPECT_CALL(*service, IsConnecting()).WillOnce(Return(true));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  TriggerScan();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  Mock::VerifyAndClearExpectations(service);
  EXPECT_CALL(*service, IsConnecting()).WillOnce(Return(false));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(1);
  TriggerScan();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  Mock::VerifyAndClearExpectations(service);
  // Similarly, ignore scans when our connected service is reconnecting.
  SetPendingService(NULL);
  SetCurrentService(service);
  EXPECT_CALL(*service, IsConnecting()).WillOnce(Return(true));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  TriggerScan();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  Mock::VerifyAndClearExpectations(service);
  // But otherwise we'll honor the request.
  EXPECT_CALL(*service, IsConnecting()).WillOnce(Return(false));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(1);
  TriggerScan();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  Mock::VerifyAndClearExpectations(service);
}

TEST_F(WiFiMainTest, ResumeStartsScanWhenIdle) {
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  ReportScanDone();
  ASSERT_TRUE(wifi()->IsIdle());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
  OnAfterResume();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, SuspendDoesNotStartScan) {
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  ASSERT_TRUE(wifi()->IsIdle());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  OnBeforeSuspend();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, ResumeDoesNotStartScanWhenNotIdle) {
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  WiFiServiceRefPtr service(SetupConnectedService(DBus::Path(), NULL, NULL));
  EXPECT_FALSE(wifi()->IsIdle());
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, EndsWith("already scanning or connected.")));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  OnAfterResume();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, ScanResults) {
  EXPECT_CALL(*wifi_provider(), OnEndpointAdded(_)).Times(5);
  StartWiFi();
  ReportBSS("bss0", "ssid0", "00:00:00:00:00:00", 0, 0, kNetworkModeAdHoc);
  ReportBSS(
      "bss1", "ssid1", "00:00:00:00:00:01", 1, 0, kNetworkModeInfrastructure);
  ReportBSS(
      "bss2", "ssid2", "00:00:00:00:00:02", 2, 0, kNetworkModeInfrastructure);
  ReportBSS(
      "bss3", "ssid3", "00:00:00:00:00:03", 3, 0, kNetworkModeInfrastructure);
  const uint16 frequency = 2412;
  ReportBSS("bss4", "ssid4", "00:00:00:00:00:04", 4, frequency,
            kNetworkModeAdHoc);

  const WiFi::EndpointMap &endpoints_by_rpcid = GetEndpointMap();
  EXPECT_EQ(5, endpoints_by_rpcid.size());

  WiFi::EndpointMap::const_iterator i;
  WiFiEndpointRefPtr endpoint;
  for (i = endpoints_by_rpcid.begin();
       i != endpoints_by_rpcid.end();
       ++i) {
    if (i->second->bssid_string() == "00:00:00:00:00:04")
      break;
  }
  ASSERT_TRUE(i != endpoints_by_rpcid.end());
  EXPECT_EQ(4, i->second->signal_strength());
  EXPECT_EQ(frequency, i->second->frequency());
  EXPECT_EQ("adhoc", i->second->network_mode());
}

TEST_F(WiFiMainTest, ScanCompleted) {
  StartWiFi();
  WiFiEndpointRefPtr ap0 = MakeEndpointWithMode("ssid0", "00:00:00:00:00:00",
                                                kNetworkModeAdHoc);
  WiFiEndpointRefPtr ap1 = MakeEndpoint("ssid1", "00:00:00:00:00:01");
  WiFiEndpointRefPtr ap2 = MakeEndpoint("ssid2", "00:00:00:00:00:02");
  EXPECT_CALL(*wifi_provider(), OnEndpointAdded(EndpointMatch(ap0))).Times(1);
  EXPECT_CALL(*wifi_provider(), OnEndpointAdded(EndpointMatch(ap1))).Times(1);
  EXPECT_CALL(*wifi_provider(), OnEndpointAdded(EndpointMatch(ap2))).Times(1);
  ReportBSS("bss0", ap0->ssid_string(), ap0->bssid_string(), 0, 0,
            kNetworkModeAdHoc);
  ReportBSS("bss1", ap1->ssid_string(), ap1->bssid_string(), 0, 0,
            kNetworkModeInfrastructure);
  ReportBSS("bss2", ap2->ssid_string(), ap2->bssid_string(), 0, 0,
            kNetworkModeInfrastructure);
  ReportScanDone();
  Mock::VerifyAndClearExpectations(wifi_provider());

  EXPECT_CALL(*wifi_provider(), OnEndpointAdded(_)).Times(0);

  // BSSes with SSIDs that start with NULL should be filtered.
  ReportBSS("bss3", string(1, 0), "00:00:00:00:00:03", 3, 0, kNetworkModeAdHoc);

  // BSSes with empty SSIDs should be filtered.
  ReportBSS("bss3", string(), "00:00:00:00:00:03", 3, 0, kNetworkModeAdHoc);
}

TEST_F(WiFiMainTest, LoneBSSRemovedWhileConnected) {
  StartWiFi();
  WiFiEndpointRefPtr endpoint;
  DBus::Path bss_path;
  WiFiServiceRefPtr service(
      SetupConnectedService(DBus::Path(), &endpoint, &bss_path));
  scoped_ptr<EndpointRemovalHandler> handler(
      MakeEndpointRemovalHandler(service));
  EXPECT_CALL(*wifi_provider(), OnEndpointRemoved(EndpointMatch(endpoint)))
      .WillOnce(Invoke(handler.get(),
                &EndpointRemovalHandler::OnEndpointRemoved));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  RemoveBSS(bss_path);
}

TEST_F(WiFiMainTest, NonSolitaryBSSRemoved) {
  StartWiFi();
  WiFiEndpointRefPtr endpoint;
  DBus::Path bss_path;
  WiFiServiceRefPtr service(
      SetupConnectedService(DBus::Path(), &endpoint, &bss_path));
  EXPECT_CALL(*wifi_provider(), OnEndpointRemoved(EndpointMatch(endpoint)))
     .WillOnce(Return(reinterpret_cast<WiFiService *>(NULL)));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect()).Times(0);
  RemoveBSS(bss_path);
}

TEST_F(WiFiMainTest, ReconnectPreservesDBusPath) {
  StartWiFi();
  DBus::Path kPath = "/test/path";
  WiFiServiceRefPtr service(SetupConnectedService(kPath, NULL, NULL));

  // Return the service to a connectable state.
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  InitiateDisconnect(service);
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());

  // Complete the disconnection by reporting a BSS change.
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);

  // A second connection attempt should remember the DBus path associated
  // with this service.
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), AddNetwork(_)).Times(0);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), SelectNetwork(kPath));
  InitiateConnect(service);
}

TEST_F(WiFiMainTest, DisconnectPendingService) {
  StartWiFi();
  MockWiFiServiceRefPtr service(
      SetupConnectingService(DBus::Path(), NULL, NULL));
  EXPECT_TRUE(GetPendingService() == service.get());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  EXPECT_CALL(*service, SetState(Service::kStateIdle)).Times(AtLeast(1));
  InitiateDisconnect(service);
  Mock::VerifyAndClearExpectations(service.get());
  EXPECT_TRUE(GetPendingService() == NULL);
}

TEST_F(WiFiMainTest, DisconnectPendingServiceWithCurrent) {
  StartWiFi();
  MockWiFiServiceRefPtr service0(
      SetupConnectedService(DBus::Path(), NULL, NULL));
  EXPECT_EQ(service0, GetCurrentService());
  EXPECT_EQ(NULL, GetPendingService().get());

  // We don't explicitly call Disconnect() while transitioning to a new
  // service.  Instead, we use the side-effect of SelectNetwork (verified in
  // SetupConnectingService).
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect()).Times(0);
  MockWiFiServiceRefPtr service1(
      SetupConnectingService("/new/path", NULL, NULL));
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());

  EXPECT_EQ(service0, GetCurrentService());
  EXPECT_EQ(service1, GetPendingService());
  EXPECT_CALL(*service1, SetState(Service::kStateIdle)).Times(AtLeast(1));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  InitiateDisconnect(service1);
  Mock::VerifyAndClearExpectations(service1.get());

  // |current_service_| will be unchanged until supplicant signals
  // that CurrentBSS has changed.
  EXPECT_EQ(service0, GetCurrentService());
  // |pending_service_| is updated immediately.
  EXPECT_EQ(NULL, GetPendingService().get());
  EXPECT_TRUE(GetPendingTimeout().IsCancelled());
}

TEST_F(WiFiMainTest, DisconnectCurrentService) {
  StartWiFi();
  ::DBus::Path kPath("/fake/path");
  MockWiFiServiceRefPtr service(SetupConnectedService(kPath, NULL, NULL));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  InitiateDisconnect(service);

  // |current_service_| should not change until supplicant reports
  // a BSS change.
  EXPECT_EQ(service, GetCurrentService());

  // Expect that the entry associated with this network will be disabled.
  MockSupplicantNetworkProxy *network_proxy = CreateSupplicantNetworkProxy();
  EXPECT_CALL(*proxy_factory(), CreateSupplicantNetworkProxy(
      kPath, WPASupplicant::kDBusAddr))
      .WillOnce(Return(network_proxy));
  EXPECT_CALL(*network_proxy, SetEnabled(false));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(kPath)).Times(0);
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
  EXPECT_EQ(NULL, GetCurrentService().get());
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
}

TEST_F(WiFiMainTest, DisconnectCurrentServiceWithErrors) {
  StartWiFi();
  DBus::Path kPath("/fake/path");
  WiFiServiceRefPtr service(SetupConnectedService(kPath, NULL, NULL));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect())
      .WillOnce(InvokeWithoutArgs(this, (&WiFiMainTest::ThrowDBusError)));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(kPath)).Times(1);
  InitiateDisconnect(service);

  // We may sometimes fail to disconnect via supplicant, and we patch up some
  // state when this happens.
  EXPECT_EQ(NULL, GetCurrentService().get());
  EXPECT_EQ(NULL, wifi()->selected_service().get());
}

TEST_F(WiFiMainTest, DisconnectCurrentServiceWithPending) {
  StartWiFi();
  WiFiServiceRefPtr service0(SetupConnectedService(DBus::Path(), NULL, NULL));
  WiFiServiceRefPtr service1(SetupConnectingService(DBus::Path(), NULL, NULL));
  EXPECT_EQ(service0, GetCurrentService());
  EXPECT_EQ(service1, GetPendingService());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect()).Times(0);
  InitiateDisconnect(service0);

  EXPECT_EQ(service0, GetCurrentService());
  EXPECT_EQ(service1, GetPendingService());
  EXPECT_FALSE(GetPendingTimeout().IsCancelled());
}

TEST_F(WiFiMainTest, TimeoutPendingService) {
  StartWiFi();
  const base::CancelableClosure &pending_timeout = GetPendingTimeout();
  EXPECT_TRUE(pending_timeout.IsCancelled());
  MockWiFiServiceRefPtr service(
      SetupConnectingService(DBus::Path(), NULL, NULL));
  EXPECT_FALSE(pending_timeout.IsCancelled());
  EXPECT_EQ(service, GetPendingService());
  EXPECT_CALL(*service, DisconnectWithFailure(Service::kFailureOutOfRange, _));
  pending_timeout.callback().Run();
}

TEST_F(WiFiMainTest, DisconnectInvalidService) {
  StartWiFi();
  MockWiFiServiceRefPtr service;
  MakeNewEndpointAndService(0, 0, kNetworkModeAdHoc, NULL, &service);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect()).Times(0);
  InitiateDisconnect(service);
}

TEST_F(WiFiMainTest, DisconnectCurrentServiceFailure) {
  StartWiFi();
  DBus::Path kPath("/fake/path");
  WiFiServiceRefPtr service(SetupConnectedService(kPath, NULL, NULL));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect())
      .WillRepeatedly(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.NotConnected",
              "test threw fi.w1.wpa_supplicant1.NotConnected")));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(kPath));
  InitiateDisconnect(service);
  EXPECT_EQ(NULL, GetCurrentService().get());
}

TEST_F(WiFiMainTest, Stop) {
  StartWiFi();
  WiFiEndpointRefPtr endpoint0;
  ::DBus::Path kPath("/fake/path");
  WiFiServiceRefPtr service0(SetupConnectedService(kPath, &endpoint0, NULL));
  WiFiEndpointRefPtr endpoint1;
  MakeNewEndpointAndService(0, 0, kNetworkModeAdHoc, &endpoint1, NULL);

  EXPECT_CALL(*wifi_provider(), OnEndpointRemoved(EndpointMatch(endpoint0)))
     .WillOnce(Return(reinterpret_cast<WiFiService *>(NULL)));
  EXPECT_CALL(*wifi_provider(), OnEndpointRemoved(EndpointMatch(endpoint1)))
     .WillOnce(Return(reinterpret_cast<WiFiService *>(NULL)));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(kPath)).Times(1);
  StopWiFi();
  EXPECT_TRUE(GetScanTimer().IsCancelled());
  EXPECT_FALSE(wifi()->weak_ptr_factory_.HasWeakPtrs());
}

TEST_F(WiFiMainTest, StopWhileConnected) {
  StartWiFi();
  WiFiEndpointRefPtr endpoint;
  WiFiServiceRefPtr service(
      SetupConnectedService(DBus::Path(), &endpoint, NULL));
  scoped_ptr<EndpointRemovalHandler> handler(
      MakeEndpointRemovalHandler(service));
  EXPECT_CALL(*wifi_provider(), OnEndpointRemoved(EndpointMatch(endpoint)))
      .WillOnce(Invoke(handler.get(),
                &EndpointRemovalHandler::OnEndpointRemoved));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  StopWiFi();
  EXPECT_TRUE(GetCurrentService() == NULL);
}

TEST_F(WiFiMainTest, ReconnectTimer) {
  StartWiFi();
  MockWiFiServiceRefPtr service(
      SetupConnectedService(DBus::Path(), NULL, NULL));
  EXPECT_CALL(*service, IsConnected()).WillRepeatedly(Return(true));
  EXPECT_TRUE(GetReconnectTimeoutCallback().IsCancelled());
  ReportStateChanged(WPASupplicant::kInterfaceStateDisconnected);
  EXPECT_FALSE(GetReconnectTimeoutCallback().IsCancelled());
  ReportStateChanged(WPASupplicant::kInterfaceStateCompleted);
  EXPECT_TRUE(GetReconnectTimeoutCallback().IsCancelled());
  ReportStateChanged(WPASupplicant::kInterfaceStateDisconnected);
  EXPECT_FALSE(GetReconnectTimeoutCallback().IsCancelled());
  ReportCurrentBSSChanged(kBSSName);
  EXPECT_TRUE(GetReconnectTimeoutCallback().IsCancelled());
  ReportStateChanged(WPASupplicant::kInterfaceStateDisconnected);
  EXPECT_FALSE(GetReconnectTimeoutCallback().IsCancelled());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  GetReconnectTimeoutCallback().callback().Run();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  EXPECT_TRUE(GetReconnectTimeoutCallback().IsCancelled());
}


MATCHER_P(HasHiddenSSID, ssid, "") {
  map<string, DBus::Variant>::const_iterator it =
      arg.find(WPASupplicant::kPropertyScanSSIDs);
  if (it == arg.end()) {
    return false;
  }

  const DBus::Variant &ssids_variant = it->second;
  EXPECT_TRUE(DBusAdaptor::IsByteArrays(ssids_variant.signature()));
  const ByteArrays &ssids = it->second.operator ByteArrays();
  // A valid Scan containing a single hidden SSID should contain
  // two SSID entries: one containing the SSID we are looking for,
  // and an empty entry, signifying that we also want to do a
  // broadcast probe request for all non-hidden APs as well.
  return ssids.size() == 2 && ssids[0] == ssid && ssids[1].empty();
}

MATCHER(HasNoHiddenSSID, "") {
  map<string, DBus::Variant>::const_iterator it =
      arg.find(WPASupplicant::kPropertyScanSSIDs);
  return it == arg.end();
}

TEST_F(WiFiMainTest, ScanHidden) {
  vector<uint8_t>kSSID(1, 'a');
  ByteArrays ssids;
  ssids.push_back(kSSID);

  StartWiFi();
  EXPECT_CALL(*wifi_provider(), GetHiddenSSIDList()).WillOnce(Return(ssids));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(HasHiddenSSID(kSSID)));
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, ScanNoHidden) {
  StartWiFi();
  EXPECT_CALL(*wifi_provider(), GetHiddenSSIDList())
      .WillOnce(Return(ByteArrays()));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(HasNoHiddenSSID()));
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, ScanWiFiDisabledAfterResume) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, EndsWith(
      "Ignoring scan request while device is not enabled."))).Times(1);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  StartWiFi();
  StopWiFi();
  // A scan is queued when WiFi resumes.
  OnAfterResume();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, InitialSupplicantState) {
  EXPECT_EQ(WiFi::kInterfaceStateUnknown, GetSupplicantState());
}

TEST_F(WiFiMainTest, StateChangeNoService) {
  // State change should succeed even if there is no pending Service.
  ReportStateChanged(WPASupplicant::kInterfaceStateScanning);
  EXPECT_EQ(WPASupplicant::kInterfaceStateScanning, GetSupplicantState());
}

TEST_F(WiFiMainTest, StateChangeWithService) {
  // Forward transition should trigger a Service state change.
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurityNone);
  InitiateConnect(service);
  EXPECT_CALL(*service.get(), SetState(Service::kStateAssociating));
  ReportStateChanged(WPASupplicant::kInterfaceStateAssociated);
  // Verify expectations now, because WiFi may report other state changes
  // when WiFi is Stop()-ed (during TearDown()).
  Mock::VerifyAndClearExpectations(service.get());
  EXPECT_CALL(*service.get(), SetState(_)).Times(AnyNumber());
}

TEST_F(WiFiMainTest, StateChangeBackwardsWithService) {
  // Some backwards transitions should not trigger a Service state change.
  // Supplicant state should still be updated, however.
  EXPECT_CALL(*dhcp_provider(), CreateConfig(_, _, _, _)).Times(AnyNumber());
  EXPECT_CALL(*dhcp_config_.get(), RequestIP()).Times(AnyNumber());
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurityNone);
  EXPECT_CALL(*service.get(), SetState(Service::kStateAssociating));
  EXPECT_CALL(*service.get(), SetState(Service::kStateConfiguring));
  InitiateConnect(service);
  ReportStateChanged(WPASupplicant::kInterfaceStateCompleted);
  ReportStateChanged(WPASupplicant::kInterfaceStateAuthenticating);
  EXPECT_EQ(WPASupplicant::kInterfaceStateAuthenticating,
            GetSupplicantState());
  // Verify expectations now, because WiFi may report other state changes
  // when WiFi is Stop()-ed (during TearDown()).
  Mock::VerifyAndClearExpectations(service);
  EXPECT_CALL(*service, SetState(_)).Times(AnyNumber());
}

TEST_F(WiFiMainTest, ConnectToServiceWithoutRecentIssues) {
  MockSupplicantProcessProxy *process_proxy = supplicant_process_proxy_.get();
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurityNone);
  EXPECT_CALL(*process_proxy, GetDebugLevel()).Times(0);
  EXPECT_CALL(*process_proxy, SetDebugLevel(_)).Times(0);
  EXPECT_CALL(*service.get(), HasRecentConnectionIssues())
      .WillOnce(Return(false));
  InitiateConnect(service);
}

TEST_F(WiFiMainTest, ConnectToServiceWithRecentIssues) {
  // Turn of WiFi debugging, so the only reason we will turn on supplicant
  // debugging will be to debug a problematic connection.
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");

  MockSupplicantProcessProxy *process_proxy = supplicant_process_proxy_.get();
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurityNone);
  EXPECT_CALL(*process_proxy, GetDebugLevel())
      .WillOnce(Return(WPASupplicant::kDebugLevelInfo));
  EXPECT_CALL(*process_proxy, SetDebugLevel(WPASupplicant::kDebugLevelDebug))
      .Times(1);
  EXPECT_CALL(*service.get(), HasRecentConnectionIssues())
      .WillOnce(Return(true));
  InitiateConnect(service);
  Mock::VerifyAndClearExpectations(process_proxy);

  SetPendingService(NULL);
  SetCurrentService(service);

  // When we disconnect from the troubled service, we should reduce the
  // level of supplciant debugging.
  EXPECT_CALL(*process_proxy, GetDebugLevel())
      .WillOnce(Return(WPASupplicant::kDebugLevelDebug));
  EXPECT_CALL(*process_proxy, SetDebugLevel(WPASupplicant::kDebugLevelInfo))
      .Times(1);
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
}

TEST_F(WiFiMainTest, CurrentBSSChangeConnectedToDisconnected) {
  StartWiFi();
  WiFiEndpointRefPtr endpoint;
  MockWiFiServiceRefPtr service =
      SetupConnectedService(DBus::Path(), &endpoint, NULL);

  EXPECT_CALL(*service, SetState(Service::kStateIdle));
  EXPECT_CALL(*service, SetFailureSilent(Service::kFailureUnknown));
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
  EXPECT_EQ(NULL, GetCurrentService().get());
  EXPECT_EQ(NULL, GetPendingService().get());
}

TEST_F(WiFiMainTest, CurrentBSSChangeConnectedToConnectedNewService) {
  StartWiFi();
  MockWiFiServiceRefPtr service0 =
      SetupConnectedService(DBus::Path(), NULL, NULL);
  MockWiFiServiceRefPtr service1;
  ::DBus::Path bss_path1(MakeNewEndpointAndService(
      0, 0, kNetworkModeAdHoc, NULL, &service1));
  EXPECT_EQ(service0.get(), GetCurrentService().get());

  // Note that we deliberately omit intermediate supplicant states
  // (e.g. kInterfaceStateAssociating), on the theory that they are
  // unreliable. Specifically, they may be quashed if the association
  // completes before supplicant flushes its changed properties.
  EXPECT_CALL(*service0, SetState(Service::kStateIdle)).Times(AtLeast(1));
  ReportCurrentBSSChanged(bss_path1);
  EXPECT_CALL(*service1, SetState(Service::kStateConfiguring));
  ReportStateChanged(WPASupplicant::kInterfaceStateCompleted);
  EXPECT_EQ(service1.get(), GetCurrentService().get());
  Mock::VerifyAndClearExpectations(service0);
  Mock::VerifyAndClearExpectations(service1);
}

TEST_F(WiFiMainTest, CurrentBSSChangedUpdateServiceEndpoint) {
  StartWiFi();
  MockWiFiServiceRefPtr service =
      SetupConnectedService(DBus::Path(), NULL, NULL);
  WiFiEndpointRefPtr endpoint;
  ::DBus::Path bss_path =
      AddEndpointToService(service, 0, 0, kNetworkModeAdHoc, &endpoint);
  EXPECT_CALL(*service, NotifyCurrentEndpoint(EndpointMatch(endpoint)));
  ReportCurrentBSSChanged(bss_path);
}

TEST_F(WiFiMainTest, NewConnectPreemptsPending) {
  StartWiFi();
  MockWiFiServiceRefPtr service0(
      SetupConnectingService(DBus::Path(), NULL, NULL));
  EXPECT_EQ(service0.get(), GetPendingService().get());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  MockWiFiServiceRefPtr service1(
      SetupConnectingService(DBus::Path(), NULL, NULL));
  EXPECT_EQ(service1.get(), GetPendingService().get());
  EXPECT_EQ(NULL, GetCurrentService().get());
}

TEST_F(WiFiMainTest, IsIdle) {
  StartWiFi();
  EXPECT_TRUE(wifi()->IsIdle());
  MockWiFiServiceRefPtr service(
      SetupConnectingService(DBus::Path(), NULL, NULL));
  EXPECT_FALSE(wifi()->IsIdle());
}

MATCHER_P(WiFiAddedArgs, bgscan, "") {
  return ContainsKey(arg, WPASupplicant::kNetworkPropertyScanSSID) &&
      ContainsKey(arg, WPASupplicant::kNetworkPropertyBgscan) == bgscan;
}

TEST_F(WiFiMainTest, AddNetworkArgs) {
  StartWiFi();
  MockWiFiServiceRefPtr service;
  MakeNewEndpointAndService(0, 0, kNetworkModeAdHoc, NULL, &service);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), AddNetwork(WiFiAddedArgs(true)));
  EXPECT_TRUE(SetBgscanMethod(WPASupplicant::kNetworkBgscanMethodSimple));
  InitiateConnect(service);
}

TEST_F(WiFiMainTest, AddNetworkArgsNoBgscan) {
  StartWiFi();
  MockWiFiServiceRefPtr service;
  MakeNewEndpointAndService(0, 0, kNetworkModeAdHoc, NULL, &service);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), AddNetwork(WiFiAddedArgs(false)));
  InitiateConnect(service);
}

TEST_F(WiFiMainTest, AppendBgscan) {
  StartWiFi();
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurityNone);
  {
    // 1 endpoint, default bgscan method -- background scan disabled.
    std::map<std::string, DBus::Variant> params;
    EXPECT_CALL(*service, GetEndpointCount()).WillOnce(Return(1));
    AppendBgscan(service, &params);
    Mock::VerifyAndClearExpectations(service);
    EXPECT_FALSE(ContainsKey(params, WPASupplicant::kNetworkPropertyBgscan));
  }
  {
    // 2 endpoints, default bgscan method -- background scan frequency reduced.
    map<string, DBus::Variant> params;
    EXPECT_CALL(*service, GetEndpointCount()).WillOnce(Return(2));
    AppendBgscan(service, &params);
    Mock::VerifyAndClearExpectations(service);
    string config_string;
    EXPECT_TRUE(
        DBusProperties::GetString(params,
                                  WPASupplicant::kNetworkPropertyBgscan,
                                  &config_string));
    vector<string> elements;
    base::SplitString(config_string, ':', &elements);
    ASSERT_EQ(4, elements.size());
    EXPECT_EQ(WiFi::kDefaultBgscanMethod, elements[0]);
    EXPECT_EQ(StringPrintf("%d", WiFi::kBackgroundScanIntervalSeconds),
              elements[3]);
  }
  {
    // Explicit bgscan method -- regular background scan frequency.
    EXPECT_TRUE(SetBgscanMethod(WPASupplicant::kNetworkBgscanMethodSimple));
    std::map<std::string, DBus::Variant> params;
    EXPECT_CALL(*service, GetEndpointCount()).Times(0);
    AppendBgscan(service, &params);
    Mock::VerifyAndClearExpectations(service);
    string config_string;
    EXPECT_TRUE(
        DBusProperties::GetString(params,
                                  WPASupplicant::kNetworkPropertyBgscan,
                                  &config_string));
    vector<string> elements;
    base::SplitString(config_string, ':', &elements);
    ASSERT_EQ(4, elements.size());
    EXPECT_EQ(StringPrintf("%d", WiFi::kDefaultScanIntervalSeconds),
              elements[3]);
  }
  {
    // No scan method, simply returns without appending properties
    EXPECT_TRUE(SetBgscanMethod(WPASupplicant::kNetworkBgscanMethodNone));
    std::map<std::string, DBus::Variant> params;
    EXPECT_CALL(*service, GetEndpointCount()).Times(0);
    AppendBgscan(service.get(), &params);
    Mock::VerifyAndClearExpectations(service);
    string config_string;
    EXPECT_FALSE(
        DBusProperties::GetString(params,
                                  WPASupplicant::kNetworkPropertyBgscan,
                                  &config_string));
  }
}

TEST_F(WiFiMainTest, StateAndIPIgnoreLinkEvent) {
  StartWiFi();
  MockWiFiServiceRefPtr service(
      SetupConnectingService(DBus::Path(), NULL, NULL));
  EXPECT_CALL(*service.get(), SetState(_)).Times(0);
  EXPECT_CALL(*dhcp_config_.get(), RequestIP()).Times(0);
  ReportLinkUp();

  // Verify expectations now, because WiFi may cause |service| state
  // changes during TearDown().
  Mock::VerifyAndClearExpectations(service);
}

TEST_F(WiFiMainTest, SupplicantCompletedAlreadyConnected) {
  StartWiFi();
  MockWiFiServiceRefPtr service(
      SetupConnectedService(DBus::Path(), NULL, NULL));
  Mock::VerifyAndClearExpectations(dhcp_config_.get());
  EXPECT_CALL(*dhcp_config_.get(), RequestIP()).Times(0);
  // Simulate a rekeying event from the AP.  These show as transitions from
  // completed->completed from wpa_supplicant.
  ReportStateChanged(WPASupplicant::kInterfaceStateCompleted);
  // When we get an IP, WiFi should enable high bitrates on the interface again.
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), EnableHighBitrates()).Times(1);
  EXPECT_CALL(*manager(), device_info()).WillOnce(Return(device_info()));
  ReportIPConfigComplete();
  // Similarly, rekeying events after we have an IP don't trigger L3
  // configuration.  However, we treat all transitions to completed as potential
  // reassociations, so we will reenable high rates again here.
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  EXPECT_CALL(*service, IsConnected()).WillOnce(Return(true));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), EnableHighBitrates()).Times(1);
  ReportStateChanged(WPASupplicant::kInterfaceStateCompleted);
}

TEST_F(WiFiMainTest, BSSAddedCreatesBSSProxy) {
  // TODO(quiche): Consider using a factory for WiFiEndpoints, so that
  // we can test the interaction between WiFi and WiFiEndpoint. (Right
  // now, we're testing across multiple layers.)
  EXPECT_CALL(*supplicant_bss_proxy_, Die()).Times(AnyNumber());
  EXPECT_CALL(*proxy_factory(), CreateSupplicantBSSProxy(_, _, _));
  StartWiFi();
  ReportBSS("bss0", "ssid0", "00:00:00:00:00:00", 0, 0, kNetworkModeAdHoc);
}

TEST_F(WiFiMainTest, BSSRemovedDestroysBSSProxy) {
  // TODO(quiche): As for BSSAddedCreatesBSSProxy, consider using a
  // factory for WiFiEndpoints.
  // Get the pointer before we transfer ownership.
  MockSupplicantBSSProxy *proxy = supplicant_bss_proxy_.get();
  EXPECT_CALL(*proxy, Die());
  StartWiFi();
  DBus::Path bss_path(
      MakeNewEndpointAndService(0, 0, kNetworkModeAdHoc, NULL, NULL));
  EXPECT_CALL(*wifi_provider(), OnEndpointRemoved(_))
     .WillOnce(Return(reinterpret_cast<WiFiService *>(NULL)));
  RemoveBSS(bss_path);
  // Check this now, to make sure RemoveBSS killed the proxy (rather
  // than TearDown).
  Mock::VerifyAndClearExpectations(proxy);
}

TEST_F(WiFiMainTest, FlushBSSOnResume) {
  const struct timeval resume_time = {1, 0};
  const struct timeval scan_done_time = {6, 0};

  StartWiFi();

  EXPECT_CALL(time_, GetTimeMonotonic(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(resume_time), Return(0)))
      .WillOnce(DoAll(SetArgumentPointee<0>(scan_done_time), Return(0)));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(),
              FlushBSS(WiFi::kMaxBSSResumeAgeSeconds + 5));
  OnAfterResume();
  ReportScanDone();
}

TEST_F(WiFiMainTest, ScanTimerIdle) {
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  ReportScanDone();
  CancelScanTimer();
  EXPECT_TRUE(GetScanTimer().IsCancelled());

  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
  FireScanTimer();
  dispatcher_.DispatchPendingEvents();
  EXPECT_FALSE(GetScanTimer().IsCancelled());  // Automatically re-armed.
}

TEST_F(WiFiMainTest, ScanTimerScanning) {
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  CancelScanTimer();
  EXPECT_TRUE(GetScanTimer().IsCancelled());

  // Should not call Scan, since we're already scanning.
  // (Scanning is triggered by StartWiFi.)
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  FireScanTimer();
  dispatcher_.DispatchPendingEvents();
  EXPECT_FALSE(GetScanTimer().IsCancelled());  // Automatically re-armed.
}

TEST_F(WiFiMainTest, ScanTimerConnecting) {
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  MockWiFiServiceRefPtr service =
      SetupConnectingService(DBus::Path(), NULL, NULL);
  CancelScanTimer();
  EXPECT_TRUE(GetScanTimer().IsCancelled());

  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  FireScanTimer();
  dispatcher_.DispatchPendingEvents();
  EXPECT_FALSE(GetScanTimer().IsCancelled());  // Automatically re-armed.
}

TEST_F(WiFiMainTest, ScanTimerReconfigured) {
  StartWiFi();
  CancelScanTimer();
  EXPECT_TRUE(GetScanTimer().IsCancelled());

  SetScanInterval(1);
  EXPECT_FALSE(GetScanTimer().IsCancelled());
}

TEST_F(WiFiMainTest, ScanTimerResetOnScanDone) {
  StartWiFi();
  CancelScanTimer();
  EXPECT_TRUE(GetScanTimer().IsCancelled());

  ReportScanDone();
  EXPECT_FALSE(GetScanTimer().IsCancelled());
}

TEST_F(WiFiMainTest, ScanTimerStopOnZeroInterval) {
  StartWiFi();
  EXPECT_FALSE(GetScanTimer().IsCancelled());

  SetScanInterval(0);
  EXPECT_TRUE(GetScanTimer().IsCancelled());
}

TEST_F(WiFiMainTest, ScanOnDisconnectWithHidden) {
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  SetupConnectedService(DBus::Path(), NULL, NULL);
  vector<uint8_t>kSSID(1, 'a');
  ByteArrays ssids;
  ssids.push_back(kSSID);
  EXPECT_CALL(*wifi_provider(), GetHiddenSSIDList())
      .WillRepeatedly(Return(ssids));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(HasHiddenSSID(kSSID)));
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, NoScanOnDisconnectWithoutHidden) {
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  SetupConnectedService(DBus::Path(), NULL, NULL);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  EXPECT_CALL(*wifi_provider(), GetHiddenSSIDList())
      .WillRepeatedly(Return(ByteArrays()));
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, LinkMonitorFailure) {
  StartWiFi();
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  MockLinkMonitor *link_monitor = new StrictMock<MockLinkMonitor>();
  SetLinkMonitor(link_monitor);
  EXPECT_CALL(*link_monitor, IsGatewayFound())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(log, Log(logging::LOG_INFO, _,
                       EndsWith("gateway was never found."))).Times(1);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Reassociate()).Times(0);
  OnLinkMonitorFailure();
  EXPECT_CALL(log, Log(logging::LOG_INFO, _,
                       EndsWith("Called Reassociate()."))).Times(1);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Reassociate()).Times(1);
  OnLinkMonitorFailure();
  OnSupplicantVanish();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Reassociate()).Times(0);
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                       EndsWith("Cannot reassociate."))).Times(1);
  OnLinkMonitorFailure();
}

TEST_F(WiFiMainTest, SuspectCredentialsOpen) {
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurityNone);
  ReportStateChanged(WPASupplicant::kInterfaceState4WayHandshake);
  EXPECT_FALSE(service->has_ever_connected());
  EXPECT_FALSE(SuspectCredentials(*service, NULL));
}

TEST_F(WiFiMainTest, SuspectCredentialsWPANeverConnected) {
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurityWpa);
  ReportStateChanged(WPASupplicant::kInterfaceState4WayHandshake);
  EXPECT_FALSE(service->has_ever_connected());
  Service::ConnectFailure failure;
  EXPECT_TRUE(SuspectCredentials(*service, &failure));
  EXPECT_EQ(Service::kFailureBadPassphrase, failure);
}

TEST_F(WiFiMainTest, SuspectCredentialsWPAPreviouslyConnected) {
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurityWpa);
  ReportStateChanged(WPASupplicant::kInterfaceState4WayHandshake);
  service->has_ever_connected_ = true;
  EXPECT_FALSE(SuspectCredentials(*service, NULL));
}

TEST_F(WiFiMainTest, SuspectCredentialsEAPInProgress) {
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurity8021x);
  SetCurrentService(service);
  service->has_ever_connected_ = false;
  EXPECT_FALSE(SuspectCredentials(*service, NULL));
  ReportEAPEvent(WPASupplicant::kEAPStatusStarted, "");
  service->has_ever_connected_ = true;
  EXPECT_FALSE(SuspectCredentials(*service, NULL));
  service->has_ever_connected_ = false;
  Service::ConnectFailure failure;
  EXPECT_TRUE(SuspectCredentials(*service, &failure));
  EXPECT_EQ(Service::kFailureEAPAuthentication, failure);
  ReportEAPEvent(WPASupplicant::kEAPStatusCompletion,
                 WPASupplicant::kEAPParameterSuccess);
  EXPECT_FALSE(SuspectCredentials(*service, NULL));
}

TEST_F(WiFiMainTest, SuspectCredentialsYieldFailureWPA) {
  ScopedMockLog log;
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurityWpa);
  SetPendingService(service);
  ReportStateChanged(WPASupplicant::kInterfaceState4WayHandshake);
  EXPECT_FALSE(service->has_ever_connected());

  EXPECT_CALL(*service, SetFailure(Service::kFailureBadPassphrase));
  EXPECT_CALL(*service, SetFailureSilent(_)).Times(0);
  EXPECT_CALL(*service, SetState(_)).Times(0);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                       EndsWith(flimflam::kErrorBadPassphrase)));
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
}

TEST_F(WiFiMainTest, SuspectCredentialsYieldFailureEAP) {
  ScopedMockLog log;
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurity8021x);
  SetCurrentService(service);
  ReportEAPEvent(WPASupplicant::kEAPStatusStarted, "");
  EXPECT_FALSE(service->has_ever_connected());

  EXPECT_CALL(*service, SetFailure(Service::kFailureEAPAuthentication));
  EXPECT_CALL(*service, SetFailureSilent(_)).Times(0);
  EXPECT_CALL(*service, SetState(_)).Times(0);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                       EndsWith(shill::kErrorEapAuthenticationFailed)));
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
}

// Scanning tests will use a mock of the event dispatcher instead of a real
// one.
class WiFiTimerTest : public WiFiObjectTest {
 public:
  WiFiTimerTest() : WiFiObjectTest(&mock_dispatcher_) {}

 protected:
  void ExpectInitialScanSequence();

  StrictMock<MockEventDispatcher> mock_dispatcher_;
};

void WiFiTimerTest::ExpectInitialScanSequence() {
  // Choose a number of iterations some multiple higher than the fast scan
  // count.
  const int kScanTimes = WiFi::kNumFastScanAttempts * 4;

  // Each time we call FireScanTimer() below, WiFi will post a task to actually
  // run Scan() on the wpa_supplicant proxy.
  EXPECT_CALL(mock_dispatcher_, PostTask(_))
      .Times(kScanTimes);
  {
    InSequence seq;
    // The scans immediately after the initial scan should happen at the short
    // interval.  If we add the initial scan (not invoked in this function) to
    // the ones in the expectation below, we get WiFi::kNumFastScanAttempts at
    // the fast scan interval.
    EXPECT_CALL(mock_dispatcher_, PostDelayedTask(
        _, WiFi::kFastScanIntervalSeconds * 1000))
        .Times(WiFi::kNumFastScanAttempts - 1)
        .WillRepeatedly(Return(true));

    // After this, the WiFi device should use the normal scan interval.
    EXPECT_CALL(mock_dispatcher_, PostDelayedTask(
        _, GetScanInterval() * 1000))
        .Times(kScanTimes - WiFi::kNumFastScanAttempts + 1)
        .WillRepeatedly(Return(true));

    for (int i = 0; i < kScanTimes; i++) {
      FireScanTimer();
    }
  }
}

TEST_F(WiFiTimerTest, FastRescan) {
  // This PostTask is a result of the call to Scan(NULL), and is meant to
  // post a task to call Scan() on the wpa_supplicant proxy immediately.
  EXPECT_CALL(mock_dispatcher_, PostTask(_));
  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(
      _, WiFi::kFastScanIntervalSeconds * 1000))
      .WillOnce(Return(true));
  StartWiFi();

  ExpectInitialScanSequence();

  // If we end up disconnecting, the sequence should repeat.
  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(
      _, WiFi::kFastScanIntervalSeconds * 1000))
      .WillOnce(Return(true));
  RestartFastScanAttempts();

  ExpectInitialScanSequence();
}

TEST_F(WiFiTimerTest, ReconnectTimer) {
  EXPECT_CALL(mock_dispatcher_, PostTask(_)).Times(AnyNumber());
  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(_, _)).Times(AnyNumber());
  StartWiFi();
  SetupConnectedService(DBus::Path(), NULL, NULL);
  Mock::VerifyAndClearExpectations(&mock_dispatcher_);

  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(
      _, GetReconnectTimeoutSeconds() * 1000)).Times(1);
  StartReconnectTimer();
  Mock::VerifyAndClearExpectations(&mock_dispatcher_);
  StopReconnectTimer();

  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(
      _, GetReconnectTimeoutSeconds() * 1000)).Times(1);
  StartReconnectTimer();
  Mock::VerifyAndClearExpectations(&mock_dispatcher_);
  GetReconnectTimeoutCallback().callback().Run();

  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(
      _, GetReconnectTimeoutSeconds() * 1000)).Times(1);
  StartReconnectTimer();
  Mock::VerifyAndClearExpectations(&mock_dispatcher_);

  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(
      _, GetReconnectTimeoutSeconds() * 1000)).Times(0);
  StartReconnectTimer();
}

TEST_F(WiFiMainTest, EAPCertification) {
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurity8021x);
  EXPECT_CALL(*service, AddEAPCertification(_, _)).Times(0);

  ScopedMockLog log;
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _, EndsWith("no current service.")));
  map<string, ::DBus::Variant> args;
  ReportCertification(args);
  Mock::VerifyAndClearExpectations(&log);

  SetCurrentService(service);
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _, EndsWith("no depth parameter.")));
  ReportCertification(args);
  Mock::VerifyAndClearExpectations(&log);

  const uint32 kDepth = 123;
  args[WPASupplicant::kInterfacePropertyDepth].writer().append_uint32(kDepth);

  EXPECT_CALL(log,
              Log(logging::LOG_ERROR, _, EndsWith("no subject parameter.")));
  ReportCertification(args);
  Mock::VerifyAndClearExpectations(&log);

  const string kSubject("subject");
  args[WPASupplicant::kInterfacePropertySubject].writer()
      .append_string(kSubject.c_str());
  EXPECT_CALL(*service, AddEAPCertification(kSubject, kDepth)).Times(1);
  ReportCertification(args);
}

TEST_F(WiFiMainTest, EAPEvent) {
  MockWiFiServiceRefPtr service = MakeMockService(flimflam::kSecurity8021x);
  EXPECT_CALL(*service, SetFailure(_)).Times(0);

  ScopedMockLog log;
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _, EndsWith("no current service.")));
  ReportEAPEvent(string(), string());
  Mock::VerifyAndClearExpectations(&log);

  SetCurrentService(service);
  const string kEAPMethod("EAP-ROCHAMBEAU");
  EXPECT_CALL(log, Log(logging::LOG_INFO, _,
                       EndsWith("accepted EAP method " + kEAPMethod)));
  ReportEAPEvent(WPASupplicant::kEAPStatusAcceptProposedMethod, kEAPMethod);

  EXPECT_CALL(log, Log(_, _, EndsWith("Completed authentication."))).Times(1);
  ReportEAPEvent(WPASupplicant::kEAPStatusCompletion,
                 WPASupplicant::kEAPParameterSuccess);

  Mock::VerifyAndClearExpectations(&log);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());

  // An EAP failure without a previous TLS indication yields a generic failure.
  Mock::VerifyAndClearExpectations(service.get());
  EXPECT_CALL(*service, DisconnectWithFailure(
      Service::kFailureEAPAuthentication, NotNull()))
      .Times(1);
  ReportEAPEvent(WPASupplicant::kEAPStatusCompletion,
                 WPASupplicant::kEAPParameterFailure);
  Mock::VerifyAndClearExpectations(service.get());

  // An EAP failure with a previous TLS indication yields a specific failure.
  SetCurrentService(service);
  EXPECT_CALL(*service, DisconnectWithFailure(_, _)).Times(0);
  EXPECT_CALL(*service, SetFailure(_)).Times(0);
  ReportEAPEvent(WPASupplicant::kEAPStatusLocalTLSAlert, string());
  Mock::VerifyAndClearExpectations(service.get());
  EXPECT_CALL(*service,
              DisconnectWithFailure(Service::kFailureEAPLocalTLS, NotNull()))
      .Times(1);
  ReportEAPEvent(WPASupplicant::kEAPStatusCompletion,
                 WPASupplicant::kEAPParameterFailure);
  Mock::VerifyAndClearExpectations(service.get());

  SetCurrentService(service);
  EXPECT_CALL(*service, DisconnectWithFailure(_, _)).Times(0);
  EXPECT_CALL(*service, SetFailure(_)).Times(0);
  ReportEAPEvent(WPASupplicant::kEAPStatusRemoteTLSAlert, string());
  Mock::VerifyAndClearExpectations(service.get());
  EXPECT_CALL(*service,
              DisconnectWithFailure(Service::kFailureEAPRemoteTLS, NotNull()))
      .Times(1);
  ReportEAPEvent(WPASupplicant::kEAPStatusCompletion,
                 WPASupplicant::kEAPParameterFailure);
  Mock::VerifyAndClearExpectations(service.get());

  SetCurrentService(service);
  EXPECT_CALL(*service, DisconnectWithFailure(_, _)).Times(0);
  EXPECT_CALL(*service, SetFailure(_)).Times(0);
  const string kStrangeParameter("ennui");
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _,EndsWith(
      string("Unexpected ") +
      WPASupplicant::kEAPStatusRemoteCertificateVerification +
      " parameter: " + kStrangeParameter)));
  ReportEAPEvent(WPASupplicant::kEAPStatusRemoteCertificateVerification,
                 kStrangeParameter);
}

TEST_F(WiFiMainTest, PendingScanDoesNotCrashAfterStop) {
  // Scan is one task that should be skipped after Stop. Others are
  // skipped by the same mechanism (invalidating weak pointers), so we
  // don't test them individually.
  //
  // Note that we can't test behavior by setting expectations on the
  // supplicant_interface_proxy_, since that is destroyed when we StopWiFi().
  StartWiFi();
  StopWiFi();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, VerifyPaths) {
  string path(WPASupplicant::kSupplicantConfPath);
  TrimString(path, FilePath::kSeparators, &path);
  EXPECT_TRUE(file_util::PathExists(FilePath(SYSROOT).Append(path)));
}

struct BSS {
  string bsspath;
  string ssid;
  string bssid;
  int16_t signal_strength;
  uint16 frequency;
  const char* mode;
};

TEST_F(WiFiMainTest, GetGeolocationObjects) {
  BSS bsses[] = {
    {"bssid1", "ssid1", "00:00:00:00:00:00", 5, Metrics::kWiFiFrequency2412,
     kNetworkModeInfrastructure},
    {"bssid2", "ssid2", "01:00:00:00:00:00", 30, Metrics::kWiFiFrequency5170,
     kNetworkModeInfrastructure},
    // Same SSID but different BSSID is an additional geolocation object.
    {"bssid3", "ssid1", "02:00:00:00:00:00", 100, 0,
     kNetworkModeInfrastructure}
  };
  StartWiFi();
  vector<GeolocationInfo> objects;
  EXPECT_EQ(objects.size(), 0);

  for (size_t i = 0; i < arraysize(bsses); ++i) {
    ReportBSS(bsses[i].bsspath, bsses[i].ssid, bsses[i].bssid,
              bsses[i].signal_strength, bsses[i].frequency, bsses[i].mode);
    objects = wifi()->GetGeolocationObjects();
    EXPECT_EQ(objects.size(), i + 1);

    GeolocationInfo expected_info;
    expected_info.AddField(kGeoMacAddressProperty, bsses[i].bssid);
    expected_info.AddField(kGeoSignalStrengthProperty,
                           StringPrintf("%d", bsses[i].signal_strength));
    expected_info.AddField(kGeoChannelProperty, StringPrintf(
        "%d", Metrics::WiFiFrequencyToChannel(bsses[i].frequency)));
    EXPECT_TRUE(objects[i].Equals(expected_info));
  };
};

TEST_F(WiFiMainTest, SetSupplicantDebugLevel) {
  MockSupplicantProcessProxy *process_proxy = supplicant_process_proxy_.get();

  // With WiFi not yet started, nothing interesting (including a crash) should
  // happen.
  EXPECT_CALL(*process_proxy, GetDebugLevel()).Times(0);
  EXPECT_CALL(*process_proxy, SetDebugLevel(_)).Times(0);
  ReportWiFiDebugScopeChanged(true);

  // This unit test turns on WiFi debugging, so when we start WiFi, we should
  // check but not set the debug level if we return the "debug" level.
  EXPECT_CALL(*process_proxy, GetDebugLevel())
      .WillOnce(Return(WPASupplicant::kDebugLevelDebug));
  EXPECT_CALL(*process_proxy, SetDebugLevel(_)).Times(0);
  StartWiFi();
  Mock::VerifyAndClearExpectations(process_proxy);

  // If WiFi debugging is toggled and wpa_supplicant reports debugging
  // is set to some unmanaged level, WiFi should leave it alone.
  EXPECT_CALL(*process_proxy, GetDebugLevel())
      .WillOnce(Return(WPASupplicant::kDebugLevelError))
      .WillOnce(Return(WPASupplicant::kDebugLevelError))
      .WillOnce(Return(WPASupplicant::kDebugLevelExcessive))
      .WillOnce(Return(WPASupplicant::kDebugLevelExcessive))
      .WillOnce(Return(WPASupplicant::kDebugLevelMsgDump))
      .WillOnce(Return(WPASupplicant::kDebugLevelMsgDump))
      .WillOnce(Return(WPASupplicant::kDebugLevelWarning))
      .WillOnce(Return(WPASupplicant::kDebugLevelWarning));
  EXPECT_CALL(*process_proxy, SetDebugLevel(_)).Times(0);
  ReportWiFiDebugScopeChanged(true);
  ReportWiFiDebugScopeChanged(false);
  ReportWiFiDebugScopeChanged(true);
  ReportWiFiDebugScopeChanged(false);
  ReportWiFiDebugScopeChanged(true);
  ReportWiFiDebugScopeChanged(false);
  ReportWiFiDebugScopeChanged(true);
  ReportWiFiDebugScopeChanged(false);
  Mock::VerifyAndClearExpectations(process_proxy);

  // If WiFi debugging is turned off and wpa_supplicant reports debugging
  // is turned on, WiFi should turn supplicant debugging off.
  EXPECT_CALL(*process_proxy, GetDebugLevel())
      .WillOnce(Return(WPASupplicant::kDebugLevelDebug));
  EXPECT_CALL(*process_proxy, SetDebugLevel(WPASupplicant::kDebugLevelInfo))
      .Times(1);
  ReportWiFiDebugScopeChanged(false);
  Mock::VerifyAndClearExpectations(process_proxy);

  // If WiFi debugging is turned on and wpa_supplicant reports debugging
  // is turned off, WiFi should turn supplicant debugging on.
  EXPECT_CALL(*process_proxy, GetDebugLevel())
      .WillOnce(Return(WPASupplicant::kDebugLevelInfo));
  EXPECT_CALL(*process_proxy, SetDebugLevel(WPASupplicant::kDebugLevelDebug))
      .Times(1);
  ReportWiFiDebugScopeChanged(true);
  Mock::VerifyAndClearExpectations(process_proxy);

  // If WiFi debugging is already in the correct state, it should not be
  // changed.
  EXPECT_CALL(*process_proxy, GetDebugLevel())
      .WillOnce(Return(WPASupplicant::kDebugLevelDebug))
      .WillOnce(Return(WPASupplicant::kDebugLevelInfo));
  EXPECT_CALL(*process_proxy, SetDebugLevel(_)).Times(0);
  ReportWiFiDebugScopeChanged(true);
  ReportWiFiDebugScopeChanged(false);

  // After WiFi is stopped, we shouldn't be calling the proxy.
  EXPECT_CALL(*process_proxy, GetDebugLevel()).Times(0);
  EXPECT_CALL(*process_proxy, SetDebugLevel(_)).Times(0);
  StopWiFi();
  ReportWiFiDebugScopeChanged(true);
  ReportWiFiDebugScopeChanged(false);
}

TEST_F(WiFiMainTest, LogSSID) {
  EXPECT_EQ("[SSID=]", WiFi::LogSSID(""));
  EXPECT_EQ("[SSID=foo\\x5b\\x09\\x5dbar]", WiFi::LogSSID("foo[\t]bar"));
}

}  // namespace shill
