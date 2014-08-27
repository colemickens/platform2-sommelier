// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi.h"

#include <linux/if.h>
#include <linux/netlink.h>  // Needs typedefs from sys/socket.h.
#include <netinet/ether.h>
#include <sys/socket.h>

#include <map>
#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>

#include "shill/dbus_adaptor.h"
#include "shill/event_dispatcher.h"
#include "shill/geolocation_info.h"
#include "shill/ieee80211.h"
#include "shill/ip_address.h"
#include "shill/ip_address_store.h"
#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_dbus_service_proxy.h"
#include "shill/mock_device.h"
#include "shill/mock_device_info.h"
#include "shill/mock_dhcp_config.h"
#include "shill/mock_dhcp_provider.h"
#include "shill/mock_eap_credentials.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_ipconfig.h"
#include "shill/mock_link_monitor.h"
#include "shill/mock_log.h"
#include "shill/mock_mac80211_monitor.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_netlink_manager.h"
#include "shill/mock_profile.h"
#include "shill/mock_proxy_factory.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/mock_scan_session.h"
#include "shill/mock_store.h"
#include "shill/mock_supplicant_bss_proxy.h"
#include "shill/mock_supplicant_eap_state_handler.h"
#include "shill/mock_supplicant_interface_proxy.h"
#include "shill/mock_supplicant_network_proxy.h"
#include "shill/mock_supplicant_process_proxy.h"
#include "shill/mock_time.h"
#include "shill/mock_wifi_provider.h"
#include "shill/mock_wifi_service.h"
#include "shill/netlink_message_matchers.h"
#include "shill/nice_mock_control.h"
#include "shill/nl80211_attribute.h"
#include "shill/nl80211_message.h"
#include "shill/property_store_unittest.h"
#include "shill/scan_session.h"
#include "shill/technology.h"
#include "shill/testing.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi_service.h"
#include "shill/wpa_supplicant.h"

using base::FilePath;
using base::StringPrintf;
using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::DefaultValue;
using ::testing::DoAll;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnNew;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::Test;
using ::testing::Throw;
using ::testing::Values;

namespace shill {

namespace {

const uint16_t kNl80211FamilyId = 0x13;
const uint16_t kRandomScanFrequency1 = 5600;
const uint16_t kRandomScanFrequency2 = 5560;
const uint16_t kRandomScanFrequency3 = 2422;
const int kInterfaceIndex = 1234;

}  // namespace

class WiFiPropertyTest : public PropertyStoreTest {
 public:
  WiFiPropertyTest()
      : metrics_(NULL),
        device_(
            new WiFi(control_interface(), dispatcher(), &metrics_,
                     manager(), "wifi", "", kInterfaceIndex)) {
  }
  virtual ~WiFiPropertyTest() {}

 protected:
  MockMetrics metrics_;
  WiFiRefPtr device_;
};

TEST_F(WiFiPropertyTest, Contains) {
  EXPECT_TRUE(device_->store().Contains(kNameProperty));
  EXPECT_FALSE(device_->store().Contains(""));
}

TEST_F(WiFiPropertyTest, SetProperty) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(
        device_->mutable_store(),
        kBgscanSignalThresholdProperty,
        PropertyStoreTest::kInt32V,
        &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                         kScanIntervalProperty,
                                         PropertyStoreTest::kUint16V,
                                         &error));
  }
  // Ensure that an attempt to write a R/O property returns InvalidArgs error.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                          kScanningProperty,
                                          PropertyStoreTest::kBoolV,
                                          &error));
    ASSERT_TRUE(error.is_set());  // name() may be invalid otherwise
    EXPECT_EQ(invalid_args(), error.name());
  }

  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(
        device_->mutable_store(),
        kBgscanMethodProperty,
        DBusAdaptor::StringToVariant(
            WPASupplicant::kNetworkBgscanMethodSimple),
        &error));
  }

  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(
        device_->mutable_store(),
        kBgscanMethodProperty,
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
      kBgscanMethodProperty, &method, &unused_error));
  EXPECT_EQ(WiFi::kDefaultBgscanMethod, method);
  EXPECT_EQ(WPASupplicant::kNetworkBgscanMethodSimple, method);

  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::SetProperty(
      device_->mutable_store(),
      kBgscanMethodProperty,
      DBusAdaptor::StringToVariant(WPASupplicant::kNetworkBgscanMethodLearn),
      &error));
  EXPECT_EQ(WPASupplicant::kNetworkBgscanMethodLearn, device_->bgscan_method_);
  EXPECT_TRUE(device_->store().GetStringProperty(
      kBgscanMethodProperty, &method, &unused_error));
  EXPECT_EQ(WPASupplicant::kNetworkBgscanMethodLearn, method);

  EXPECT_TRUE(DBusAdaptor::ClearProperty(
      device_->mutable_store(), kBgscanMethodProperty, &error));
  EXPECT_TRUE(device_->store().GetStringProperty(
      kBgscanMethodProperty, &method, &unused_error));
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
  explicit WiFiObjectTest(EventDispatcher *dispatcher)
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
                       kInterfaceIndex)),
        bss_counter_(0),
        mac80211_monitor_(
            new StrictMock<MockMac80211Monitor>(
                dispatcher, kDeviceName, WiFi::kStuckQueueLengthThreshold,
                base::Closure(), &metrics_)),
        dbus_service_proxy_(new MockDBusServiceProxy()),
        supplicant_process_proxy_(new NiceMock<MockSupplicantProcessProxy>()),
        supplicant_bss_proxy_(new NiceMock<MockSupplicantBSSProxy>()),
        dhcp_config_(new MockDHCPConfig(&control_interface_, kDeviceName)),
        dbus_manager_(new DBusManager()),
        adaptor_(new DeviceMockAdaptor()),
        eap_state_handler_(new NiceMock<MockSupplicantEAPStateHandler>()),
        supplicant_interface_proxy_(
            new NiceMock<MockSupplicantInterfaceProxy>()) {
    wifi_->mac80211_monitor_.reset(mac80211_monitor_);
    InstallMockScanSession();
    ::testing::DefaultValue< ::DBus::Path>::Set("/default/path");

    EXPECT_CALL(*mac80211_monitor_, UpdateConnectedState(_))
        .Times(AnyNumber());

    ON_CALL(dhcp_provider_, CreateConfig(_, _, _, _))
        .WillByDefault(Return(dhcp_config_));
    ON_CALL(*dhcp_config_.get(), RequestIP()).WillByDefault(Return(true));

    ON_CALL(proxy_factory_, CreateDBusServiceProxy())
        .WillByDefault(ReturnAndReleasePointee(&dbus_service_proxy_));
    ON_CALL(proxy_factory_, CreateSupplicantProcessProxy(_, _))
        .WillByDefault(ReturnAndReleasePointee(&supplicant_process_proxy_));
    ON_CALL(proxy_factory_, CreateSupplicantInterfaceProxy(_, _, _))
        .WillByDefault(ReturnAndReleasePointee(&supplicant_interface_proxy_));
    ON_CALL(proxy_factory_, CreateSupplicantBSSProxy(_, _, _))
        .WillByDefault(ReturnAndReleasePointee(&supplicant_bss_proxy_));
    ON_CALL(proxy_factory_, CreateSupplicantNetworkProxy(_, _))
        .WillByDefault(ReturnNew<NiceMock<MockSupplicantNetworkProxy>>());
    Nl80211Message::SetMessageType(kNl80211FamilyId);

    // Transfers ownership.
    manager_.dbus_manager_.reset(dbus_manager_);
    wifi_->eap_state_handler_.reset(eap_state_handler_);

    wifi_->provider_ = &wifi_provider_;
    wifi_->time_ = &time_;
    wifi_->netlink_manager_ = &netlink_manager_;
    wifi_->progressive_scan_enabled_ = true;
    wifi_->adaptor_.reset(adaptor_);  // Transfers ownership.

    // The following is only useful when a real |ScanSession| is used; it is
    // ignored by |MockScanSession|.
    wifi_->all_scan_frequencies_.insert(kRandomScanFrequency1);
    wifi_->all_scan_frequencies_.insert(kRandomScanFrequency2);
    wifi_->all_scan_frequencies_.insert(kRandomScanFrequency3);
  }

  virtual void SetUp() {
    // EnableScopes... so that we can EXPECT_CALL for scoped log messages.
    ScopeLogger::GetInstance()->EnableScopesByName("wifi");
    ScopeLogger::GetInstance()->set_verbose_level(3);
    dbus_manager_->proxy_factory_ = &proxy_factory_;
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
    EXPECT_CALL(*mac80211_monitor_, Stop());
    wifi_->proxy_factory_ = NULL;
    // must Stop WiFi instance, to clear its list of services.
    // otherwise, the WiFi instance will not be deleted. (because
    // services reference a WiFi instance, creating a cycle.)
    wifi_->Stop(NULL, ResultCallback());
    wifi_->set_dhcp_provider(NULL);
    dbus_manager_->Stop();
    dbus_manager_->proxy_factory_ = NULL;
    // Reset scope logging, to avoid interfering with other tests.
    ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
    ScopeLogger::GetInstance()->set_verbose_level(0);
  }

  // Needs to be public since it is called via Invoke().
  void StopWiFi() {
    EXPECT_CALL(*mac80211_monitor_, Stop());
    wifi_->SetEnabled(false);  // Stop(NULL, ResultCallback());
  }

  // Needs to be public since it is called via Invoke().
  void ThrowDBusError() {
    throw DBus::Error("SomeDBusType", "A handy message");
  }
  void ResetPendingService() {
    SetPendingService(NULL);
  }

  size_t GetScanFrequencyCount() const {
    return wifi_->all_scan_frequencies_.size();
  }

  void SetScanSize(int min, int max) {
    wifi_->min_frequencies_to_scan_ = min;
    wifi_->max_frequencies_to_scan_ = max;
  }

  // This clears WiFi::scan_session_, thereby allowing WiFi::Scan to create a
  // real scan session.
  void ClearScanSession() {
    wifi_->scan_session_.reset();
  }

  bool IsScanSessionNull() {
    return !wifi_->scan_session_;
  }

  void InstallMockScanSession() {
    WiFiProvider::FrequencyCountList previous_frequencies;
    std::set<uint16_t> available_frequencies;
    ScanSession::FractionList fractions;
    ScanSession::OnScanFailed null_callback;
    scan_session_ = new MockScanSession(&netlink_manager_,
                                        event_dispatcher_,
                                        previous_frequencies,
                                        available_frequencies,
                                        0,
                                        fractions,
                                        0,
                                        0,
                                        null_callback,
                                        NULL);
    wifi_->scan_session_.reset(scan_session_);
  }

  // Or DisableProgressiveScan()...
  void EnableFullScan() {
    wifi_->progressive_scan_enabled_ = false;
  }

  void OnTriggerScanResponse(const Nl80211Message &message) {
    wifi_->scan_session_->OnTriggerScanResponse(message);
  }

  void SetScanState(WiFi::ScanState new_state,
                    WiFi::ScanMethod new_method,
                    const char *reason) {
    wifi_->SetScanState(new_state, new_method, reason);
  }

  void VerifyScanState(WiFi::ScanState state, WiFi::ScanMethod method) const {
    EXPECT_EQ(state, wifi_->scan_state_);
    EXPECT_EQ(method, wifi_->scan_method_);
  }

  void SetRoamThresholdMember(uint16_t threshold) {
    wifi_->roam_threshold_db_ = threshold;
  }

  bool SetRoamThreshold(uint16_t threshold) {
    return wifi_->SetRoamThreshold(threshold, nullptr);
  }

  uint16_t GetRoamThreshold() const {
    return wifi_->GetRoamThreshold(nullptr);
  }

 protected:
  typedef scoped_refptr<MockWiFiService> MockWiFiServiceRefPtr;

  // Simulate the course of events when the last endpoint of a service is
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
      const WiFiServiceRefPtr &service) {
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
      *ssid = StringPrintf("ssid%d", bss_counter_);
    }
    *path = StringPrintf("/interface/bss%d", bss_counter_);
    *bssid = StringPrintf("00:00:00:00:00:%02x", bss_counter_);
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
        kModeManaged,
        security,
        false);
  }
  MockWiFiServiceRefPtr MakeMockService(const std::string &security) {
    return MakeMockServiceWithSSID(vector<uint8_t>(1, 'a'), security);
  }
  ::DBus::Path MakeNewEndpointAndService(int16_t signal_strength,
                                         uint16_t frequency,
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
      uint16_t frequency,
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
    wifi_->ConnectTo(service);
  }
  void InitiateDisconnect(WiFiServiceRefPtr service) {
    wifi_->DisconnectFrom(service);
  }
  void InitiateDisconnectIfActive(WiFiServiceRefPtr service) {
    wifi_->DisconnectFromIfActive(service);
  }
  MockWiFiServiceRefPtr SetupConnectingService(
      const DBus::Path &network_path,
      WiFiEndpointRefPtr *endpoint_ptr,
      ::DBus::Path *bss_path_ptr) {
    MockWiFiServiceRefPtr service;
    WiFiEndpointRefPtr endpoint;
    ::DBus::Path bss_path(MakeNewEndpointAndService(
        0, 0, kNetworkModeAdHoc, &endpoint, &service));
    if (!network_path.empty()) {
      EXPECT_CALL(*service, GetSupplicantConfigurationParameters());
      EXPECT_CALL(*GetSupplicantInterfaceProxy(), AddNetwork(_))
          .WillOnce(Return(network_path));
      EXPECT_CALL(*GetSupplicantInterfaceProxy(), SelectNetwork(network_path));
    }
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
    EXPECT_CALL(*service, ResetSuspectedCredentialFailures());
    EXPECT_CALL(*dhcp_provider(), CreateConfig(_, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(*dhcp_config_.get(), RequestIP()).Times(AnyNumber());
    EXPECT_CALL(wifi_provider_, IncrementConnectCount(_));
    ReportStateChanged(WPASupplicant::kInterfaceStateCompleted);
    Mock::VerifyAndClearExpectations(service);

    EXPECT_EQ(service, GetCurrentService());
    return service;
  }

  void FireScanTimer() {
    wifi_->ScanTimerHandler();
  }
  void TriggerScan(WiFi::ScanMethod method) {
    if (method == WiFi::kScanMethodFull) {
      wifi_->Scan(Device::kFullScan, NULL, __func__);
    } else {
      wifi_->Scan(Device::kProgressiveScan, NULL, __func__);
    }
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
  const ServiceRefPtr &GetSelectedService() {
    return wifi_->selected_service();
  }
  const string &GetSupplicantBSS() {
    return wifi_->supplicant_bss_;
  }
  void SetSupplicantBSS(const string &bss) {
    wifi_->supplicant_bss_ = bss;
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
                 uint16_t frequency,
                 const char *mode);
  void ReportIPConfigComplete() {
    wifi_->OnIPConfigUpdated(dhcp_config_);
  }
  void ReportIPConfigFailure() {
    wifi_->OnIPConfigFailure();
  }
  void ReportConnected() {
    wifi_->OnConnected();
  }
  void ReportLinkUp() {
    wifi_->LinkEvent(IFF_LOWER_UP, IFF_LOWER_UP);
  }
  void ReportScanDone() {
    // Eliminate |scan_session| so |ScanDoneTask| doesn't launch another scan.
    wifi_->scan_session_.reset();
    wifi_->ScanDoneTask();
    // Make a new |scan_session| so that future scanning is done with the mock.
    InstallMockScanSession();
  }
  void ReportScanDoneKeepScanSession() {
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
  void RequestStationInfo() {
    wifi_->RequestStationInfo();
  }
  void ReportReceivedStationInfo(const Nl80211Message &nl80211_message) {
    wifi_->OnReceivedStationInfo(nl80211_message);
  }
  KeyValueStore GetLinkStatistics() {
    return wifi_->GetLinkStatistics(NULL);
  }
  void SetPendingService(const WiFiServiceRefPtr &service) {
    wifi_->SetPendingService(service);
  }
  void SetServiceNetworkRpcId(
      const WiFiServiceRefPtr &service, const string &rpcid) {
    wifi_->rpcid_by_service_[service.get()] = rpcid;
  }
  bool SetScanInterval(uint16_t interval_seconds, Error *error) {
    return wifi_->SetScanInterval(interval_seconds, error);
  }
  uint16_t GetScanInterval() {
    return wifi_->GetScanInterval(NULL);
  }
  void StartWiFi(bool supplicant_present) {
    EXPECT_CALL(netlink_manager_, SubscribeToEvents(
        Nl80211Message::kMessageTypeString,
        NetlinkManager::kEventTypeConfig));
    EXPECT_CALL(netlink_manager_, SubscribeToEvents(
        Nl80211Message::kMessageTypeString,
        NetlinkManager::kEventTypeScan));
    EXPECT_CALL(netlink_manager_, SubscribeToEvents(
        Nl80211Message::kMessageTypeString,
        NetlinkManager::kEventTypeRegulatory));
    EXPECT_CALL(netlink_manager_, SubscribeToEvents(
        Nl80211Message::kMessageTypeString,
        NetlinkManager::kEventTypeMlme));
    EXPECT_CALL(netlink_manager_, SendNl80211Message(
        IsNl80211Command(kNl80211FamilyId, NL80211_CMD_GET_WIPHY), _, _, _));

    dbus_manager_->Start();
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
    wifi_->OnSupplicantAppear(WPASupplicant::kDBusAddr, ":1.7");
    EXPECT_TRUE(wifi_->supplicant_present_);
  }
  void OnSupplicantVanish() {
    wifi_->OnSupplicantVanish(WPASupplicant::kDBusAddr);
    EXPECT_FALSE(wifi_->supplicant_present_);
  }
  bool GetSupplicantPresent() {
    return wifi_->supplicant_present_;
  }
  bool GetIsRoamingInProgress() {
    return wifi_->is_roaming_in_progress_;
  }
  void SetIPConfig(const IPConfigRefPtr &ipconfig) {
    return wifi_->set_ipconfig(ipconfig);
  }
  bool SetBgscanMethod(const string &method) {
    ::DBus::Error error;
    return DBusAdaptor::SetProperty(
        wifi_->mutable_store(),
        kBgscanMethodProperty,
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

  bool SuspectCredentials(const WiFiServiceRefPtr &service,
                          Service::ConnectFailure *failure) {
    return wifi_->SuspectCredentials(service, failure);
  }

  void OnLinkMonitorFailure() {
    wifi_->OnLinkMonitorFailure();
  }

  bool SetBgscanShortInterval(const uint16_t &interval, Error *error) {
    return wifi_->SetBgscanShortInterval(interval, error);
  }

  bool SetBgscanSignalThreshold(const int32_t &threshold, Error *error) {
    return wifi_->SetBgscanSignalThreshold(threshold, error);
  }

  bool TDLSDiscover(const string &peer) {
    return wifi_->TDLSDiscover(peer);
  }

  bool TDLSSetup(const string &peer) {
    return wifi_->TDLSSetup(peer);
  }

  string TDLSStatus(const string &peer) {
    return wifi_->TDLSStatus(peer);
  }

  bool TDLSTeardown(const string &peer) {
    return wifi_->TDLSTeardown(peer);
  }

  string PerformTDLSOperation(const string &operation,
                              const string &peer,
                              Error *error) {
    return wifi_->PerformTDLSOperation(operation, peer, error);
  }

  void TimeoutPendingConnection() {
    wifi_->PendingTimeoutHandler();
  }

  void OnNewWiphy(const Nl80211Message &new_wiphy_message) {
    wifi_->OnNewWiphy(new_wiphy_message);
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

  MockProxyFactory *proxy_factory() {
    return &proxy_factory_;
  }

  MockWiFiProvider *wifi_provider() {
    return &wifi_provider_;
  }

  MockMac80211Monitor *mac80211_monitor() {
    return mac80211_monitor_;
  }

  IPAddressStore *GetWakeOnPacketConnections() {
    return &wifi_->wake_on_packet_connections_;
  }

  int *GetNumSetWakeOnPacketRetries() {
    return &wifi_->num_set_wake_on_packet_retries_;
  }

  void AddWakeOnPacketConnection(const IPAddress &ip_endpoint, Error *error) {
    wifi_->AddWakeOnPacketConnection(ip_endpoint, error);
  }

  void RemoveWakeOnPacketConnection(const IPAddress &ip_endpoint,
                                    Error *error) {
    wifi_->RemoveWakeOnPacketConnection(ip_endpoint, error);
  }

  void RemoveAllWakeOnPacketConnections(Error *error) {
    wifi_->RemoveAllWakeOnPacketConnections(error);
  }

  void RequestWakeOnPacketSettings() { wifi_->RequestWakeOnPacketSettings(); }

  void VerifyWakeOnPacketConnections(const Nl80211Message &nl80211_message) {
    wifi_->VerifyWakeOnPacketConnections(nl80211_message);
  }

  void SendSetWakeOnPacketMessage(Error *error) {
    wifi_->SendSetWakeOnPacketMessage(error);
  }

  void RetrySetWakeOnPacketConnections() {
    wifi_->RetrySetWakeOnPacketConnections();
  }

  EventDispatcher *event_dispatcher_;
  MockScanSession *scan_session_;  // Owned by |wifi_|.
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
  MockMac80211Monitor *mac80211_monitor_;  // Owned by |wifi_|.

  // protected fields interspersed between private fields, due to
  // initialization order
 protected:
  static const char kDeviceName[];
  static const char kDeviceAddress[];
  static const char kNetworkModeAdHoc[];
  static const char kNetworkModeInfrastructure[];
  static const char kBSSName[];
  static const char kSSIDName[];
  static const uint16_t kRoamThreshold;

  scoped_ptr<MockDBusServiceProxy> dbus_service_proxy_;
  scoped_ptr<MockSupplicantProcessProxy> supplicant_process_proxy_;
  scoped_ptr<MockSupplicantBSSProxy> supplicant_bss_proxy_;
  MockDHCPProvider dhcp_provider_;
  scoped_refptr<MockDHCPConfig> dhcp_config_;
  DBusManager *dbus_manager_;

  // These pointers track mock objects owned by the WiFi device instance
  // and manager so we can perform expectations against them.
  DeviceMockAdaptor *adaptor_;
  MockSupplicantEAPStateHandler *eap_state_handler_;
  MockNetlinkManager netlink_manager_;

 private:
  scoped_ptr<MockSupplicantInterfaceProxy> supplicant_interface_proxy_;
  MockProxyFactory proxy_factory_;
};

const char WiFiObjectTest::kDeviceName[] = "wlan0";
const char WiFiObjectTest::kDeviceAddress[] = "000102030405";
const char WiFiObjectTest::kNetworkModeAdHoc[] = "ad-hoc";
const char WiFiObjectTest::kNetworkModeInfrastructure[] = "infrastructure";
const char WiFiObjectTest::kBSSName[] = "bss0";
const char WiFiObjectTest::kSSIDName[] = "ssid0";
const uint16_t WiFiObjectTest::kRoamThreshold = 32;  // Arbitrary value.

void WiFiObjectTest::RemoveBSS(const ::DBus::Path &bss_path) {
  wifi_->BSSRemovedTask(bss_path);
}

void WiFiObjectTest::ReportBSS(const ::DBus::Path &bss_path,
                             const string &ssid,
                             const string &bssid,
                             int16_t signal_strength,
                             uint16_t frequency,
                             const char *mode) {
  map<string, ::DBus::Variant> bss_properties;

  {
    DBus::MessageIter writer(bss_properties["SSID"].writer());
    writer << vector<uint8_t>(ssid.begin(), ssid.end());
  }
  {
    string bssid_nosep;
    vector<uint8_t> bssid_bytes;
    base::RemoveChars(bssid, ":", &bssid_nosep);
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

// Most of our tests involve using a real EventDispatcher object.
class WiFiMainTest : public WiFiObjectTest {
 public:
  WiFiMainTest() : WiFiObjectTest(&dispatcher_) {}

 protected:
  // A progressive scan requests one or more scans, each of which asks about a
  // different batch of frequencies/channels.
  enum WhichBatchOfProgressiveScan {
    kFirstProgressiveScanBatch,
    kOnlyFullScanBatch,
    kNotFirstProgressiveScanBatch
  };
  void StartScan(WiFi::ScanMethod method) {
    if (method == WiFi::kScanMethodFull) {
      EnableFullScan();
    }
    VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);
    EXPECT_CALL(*adaptor_, EmitBoolChanged(kPoweredProperty, _)).
        Times(AnyNumber());
    // Using kFirstProgressiveScanBatch regardless of the method since
    // kFOnlyFullScanBatch does exactly the same thing.
    ExpectScanStart(method, false);
    StartWiFi();
    dispatcher_.DispatchPendingEvents();
    VerifyScanState(WiFi::kScanScanning, method);
  }

  MockWiFiServiceRefPtr AttemptConnection(WiFi::ScanMethod method,
                                          WiFiEndpointRefPtr *endpoint,
                                          ::DBus::Path *bss_path) {
    WiFiEndpointRefPtr dummy_endpoint;
    if (!endpoint) {
      endpoint = &dummy_endpoint;  // If caller doesn't care about endpoint.
    }

    ::DBus::Path dummy_bss_path;
    if (!bss_path) {
      bss_path = &dummy_bss_path;  // If caller doesn't care about bss_path.
    }

    ExpectScanStop();
    ExpectConnecting();
    MockWiFiServiceRefPtr service =
        SetupConnectingService(DBus::Path(), endpoint, bss_path);
    ReportScanDoneKeepScanSession();
    dispatcher_.DispatchPendingEvents();
    VerifyScanState(WiFi::kScanConnecting, method);

    return service;
  }

  void ExpectScanStart(WiFi::ScanMethod method, bool is_continued) {
    if (method == WiFi::kScanMethodProgressive) {
      ASSERT_FALSE(IsScanSessionNull());
      EXPECT_CALL(*scan_session_, HasMoreFrequencies());
      EXPECT_CALL(*scan_session_, InitiateScan());
    } else {
      EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
    }
    if (!is_continued) {
      EXPECT_CALL(*adaptor_, EmitBoolChanged(kScanningProperty,
                                             true));
      EXPECT_CALL(*metrics(), NotifyDeviceScanStarted(_));
    }
  }

  // Scanning can stop for any reason (including transitioning to connecting).
  void ExpectScanStop() {
    EXPECT_CALL(*adaptor_, EmitBoolChanged(kScanningProperty, false));
  }

  void ExpectConnecting() {
    EXPECT_CALL(*metrics(), NotifyDeviceScanFinished(_));
    EXPECT_CALL(*metrics(), NotifyDeviceConnectStarted(_, _));
  }

  void ExpectConnected() {
    EXPECT_CALL(*metrics(), NotifyDeviceConnectFinished(_));
    ExpectScanIdle();
  }

  void ExpectFoundNothing() {
    EXPECT_CALL(*metrics(), NotifyDeviceScanFinished(_));
    EXPECT_CALL(*metrics(), ResetConnectTimer(_));
    ExpectScanIdle();
  }

  void ExpectScanIdle() {
    EXPECT_CALL(*metrics(), ResetScanTimer(_));
    EXPECT_CALL(*metrics(), ResetConnectTimer(_)).RetiresOnSaturation();
  }

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

TEST_F(WiFiMainTest, RoamThresholdProperty) {
  static const uint16_t kRoamThreshold16 = 16;
  static const uint16_t kRoamThreshold32 = 32;

  StartWiFi(false);  // No supplicant present.
  OnSupplicantAppear();

  EXPECT_CALL(*GetSupplicantInterfaceProxy(),
              SetRoamThreshold(kRoamThreshold16));
  EXPECT_TRUE(SetRoamThreshold(kRoamThreshold16));
  EXPECT_EQ(GetRoamThreshold(), kRoamThreshold16);

  // Try a different number
  EXPECT_CALL(*GetSupplicantInterfaceProxy(),
              SetRoamThreshold(kRoamThreshold32));
  EXPECT_TRUE(SetRoamThreshold(kRoamThreshold32));
  EXPECT_EQ(GetRoamThreshold(), kRoamThreshold32);
}

TEST_F(WiFiMainTest, OnSupplicantAppearStarted) {
  EXPECT_TRUE(GetSupplicantProcessProxy() == NULL);

  EXPECT_CALL(*dbus_service_proxy_.get(),
              GetNameOwner(WPASupplicant::kDBusAddr, _, _, _));
  StartWiFi(false);  // No supplicant present.
  EXPECT_TRUE(GetSupplicantProcessProxy() == NULL);

  SetRoamThresholdMember(kRoamThreshold);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveAllNetworks());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), FlushBSS(0));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), SetFastReauth(false));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), SetRoamThreshold(kRoamThreshold));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), SetScanInterval(_));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), SetDisableHighBitrates(true));

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

TEST_F(WiFiMainTest, CleanStart_FullScan) {
  EnableFullScan();
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
  EXPECT_CALL(*scan_session_, InitiateScan());
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
  StartWiFi();

  // With no selected service.
  EXPECT_TRUE(wifi()->ShouldUseArpGateway());
  EXPECT_CALL(dhcp_provider_, CreateConfig(kDeviceName, _, _, true))
      .WillOnce(Return(dhcp_config_));
  const_cast<WiFi *>(wifi().get())->AcquireIPConfig();

  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
  InitiateConnect(service);

  // Selected service that does not have a static IP address.
  EXPECT_CALL(*service, HasStaticIPAddress()).WillRepeatedly(Return(false));
  EXPECT_TRUE(wifi()->ShouldUseArpGateway());
  EXPECT_CALL(dhcp_provider_, CreateConfig(kDeviceName, _, _, true))
      .WillOnce(Return(dhcp_config_));
  const_cast<WiFi *>(wifi().get())->AcquireIPConfig();
  Mock::VerifyAndClearExpectations(service);

  // Selected service that has a static IP address.
  EXPECT_CALL(*service, HasStaticIPAddress()).WillRepeatedly(Return(true));
  EXPECT_FALSE(wifi()->ShouldUseArpGateway());
  EXPECT_CALL(dhcp_provider_, CreateConfig(kDeviceName, _, _, false))
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

TEST_F(WiFiMainTest, Restart_FullScan) {
  EnableFullScan();
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

TEST_F(WiFiMainTest, Restart) {
  EXPECT_CALL(*supplicant_process_proxy_, CreateInterface(_))
      .Times(AnyNumber())
      .WillRepeatedly(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.InterfaceExists",
              "test threw fi.w1.wpa_supplicant1.InterfaceExists")));
  EXPECT_CALL(*scan_session_, InitiateScan());
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, StartClearsState) {
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveAllNetworks());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), FlushBSS(_));
  StartWiFi();
}

TEST_F(WiFiMainTest, NoScansWhileConnecting_FullScan) {
  // Setup 'connecting' state.
  StartScan(WiFi::kScanMethodFull);
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());

  ExpectScanStop();
  ExpectConnecting();
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
  InitiateConnect(service);
  VerifyScanState(WiFi::kScanConnecting, WiFi::kScanMethodFull);

  // If we're connecting, we ignore scan requests and stay on channel.
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  TriggerScan(WiFi::kScanMethodFull);
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  Mock::VerifyAndClearExpectations(service);

  // Terminate the scan.
  ExpectFoundNothing();
  TimeoutPendingConnection();
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);

  // Start a fresh scan.
  ExpectScanStart(WiFi::kScanMethodFull, false);
  TriggerScan(WiFi::kScanMethodFull);
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  Mock::VerifyAndClearExpectations(service);

  // Similarly, ignore scans when our connected service is reconnecting.
  ExpectScanStop();
  ExpectScanIdle();
  SetPendingService(NULL);
  SetCurrentService(service);
  EXPECT_CALL(*service, IsConnecting()).WillOnce(Return(true));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  TriggerScan(WiFi::kScanMethodFull);
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  Mock::VerifyAndClearExpectations(service);

  // But otherwise we'll honor the request.
  EXPECT_CALL(*service, IsConnecting()).Times(AtLeast(2)).
      WillRepeatedly(Return(false));
  ExpectScanStart(WiFi::kScanMethodFull, false);
  TriggerScan(WiFi::kScanMethodFull);
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  Mock::VerifyAndClearExpectations(service);

  // Silence messages from the destructor.
  ExpectScanStop();
  ExpectScanIdle();
}

TEST_F(WiFiMainTest, NoScansWhileConnecting) {
  // Setup 'connecting' state.
  StartScan(WiFi::kScanMethodProgressive);
  ExpectScanStop();
  ExpectConnecting();
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
  InitiateConnect(service);
  VerifyScanState(WiFi::kScanConnecting, WiFi::kScanMethodProgressive);

  // If we're connecting, we ignore scan requests and stay on channel.
  EXPECT_CALL(*scan_session_, InitiateScan()).Times(0);
  TriggerScan(WiFi::kScanMethodProgressive);
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(service);
  Mock::VerifyAndClearExpectations(scan_session_);

  // Terminate the scan.
  ExpectFoundNothing();
  TimeoutPendingConnection();
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);

  // Start a fresh scan.
  InstallMockScanSession();
  ExpectScanStart(WiFi::kScanMethodProgressive, false);
  TriggerScan(WiFi::kScanMethodProgressive);
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(service);
  Mock::VerifyAndClearExpectations(scan_session_);

  // Similarly, ignore scans when our connected service is reconnecting.
  ExpectScanStop();
  ExpectScanIdle();
  SetPendingService(NULL);
  SetCurrentService(service);
  EXPECT_CALL(*service, IsConnecting()).WillOnce(Return(true));
  InstallMockScanSession();
  EXPECT_CALL(*scan_session_, InitiateScan()).Times(0);
  TriggerScan(WiFi::kScanMethodProgressive);
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(service);
  Mock::VerifyAndClearExpectations(scan_session_);

  // Unlike Full scan, Progressive scan will reject attempts to scan while
  // we're connected.
  EXPECT_CALL(*service, IsConnecting()).WillOnce(Return(false));
  EXPECT_CALL(*scan_session_, InitiateScan()).Times(0);
  TriggerScan(WiFi::kScanMethodProgressive);
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(service);
  Mock::VerifyAndClearExpectations(scan_session_);
}

TEST_F(WiFiMainTest, ResumeStartsScanWhenIdle_FullScan) {
  EnableFullScan();
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

TEST_F(WiFiMainTest, ResumeStartsScanWhenIdle) {
  EXPECT_CALL(*scan_session_, InitiateScan());
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  ReportScanDone();
  ASSERT_TRUE(wifi()->IsIdle());
  dispatcher_.DispatchPendingEvents();
  OnAfterResume();
  EXPECT_TRUE(scan_session_ != NULL);
  InstallMockScanSession();
  EXPECT_CALL(*scan_session_, InitiateScan());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, SuspendDoesNotStartScan_FullScan) {
  EnableFullScan();
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  ASSERT_TRUE(wifi()->IsIdle());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  OnBeforeSuspend();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, SuspendDoesNotStartScan) {
  EXPECT_CALL(*scan_session_, InitiateScan());
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  ASSERT_TRUE(wifi()->IsIdle());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  EXPECT_CALL(*scan_session_, InitiateScan()).Times(0);
  OnBeforeSuspend();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, ResumeDoesNotStartScanWhenNotIdle_FullScan) {
  EnableFullScan();
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  WiFiServiceRefPtr service(SetupConnectedService(DBus::Path(), NULL, NULL));
  EXPECT_FALSE(wifi()->IsIdle());
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, EndsWith("already connecting or connected.")));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  OnAfterResume();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, ResumeDoesNotStartScanWhenNotIdle) {
  EXPECT_CALL(*scan_session_, InitiateScan());
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  WiFiServiceRefPtr service(SetupConnectedService(DBus::Path(), NULL, NULL));
  EXPECT_FALSE(wifi()->IsIdle());
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, EndsWith("already connecting or connected.")));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  EXPECT_TRUE(IsScanSessionNull());
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
  const uint16_t frequency = 2412;
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
  MockWiFiServiceRefPtr service(SetupConnectedService(kPath, NULL, NULL));

  // Return the service to a connectable state.
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  InitiateDisconnect(service);
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());

  // Complete the disconnection by reporting a BSS change.
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);

  // A second connection attempt should remember the DBus path associated
  // with this service, and should not request new configuration parameters.
  EXPECT_CALL(*service, GetSupplicantConfigurationParameters()).Times(0);
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
  EXPECT_CALL(*service, SetFailure(_)).Times(0);
  EXPECT_CALL(*service, SetState(Service::kStateIdle)).Times(AtLeast(1));
  service->set_expecting_disconnect(true);
  InitiateDisconnect(service);
  Mock::VerifyAndClearExpectations(service.get());
  EXPECT_TRUE(GetPendingService() == NULL);
}

TEST_F(WiFiMainTest, DisconnectPendingServiceWithFailure) {
  StartWiFi();
  MockWiFiServiceRefPtr service(
      SetupConnectingService(DBus::Path(), NULL, NULL));
  EXPECT_TRUE(GetPendingService() == service.get());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  EXPECT_CALL(*service, SetFailure(Service::kFailureOutOfRange));
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
  service->set_expecting_disconnect(true);
  InitiateDisconnect(service);

  // |current_service_| should not change until supplicant reports
  // a BSS change.
  EXPECT_EQ(service, GetCurrentService());

  // Expect that the entry associated with this network will be disabled.
  scoped_ptr<MockSupplicantNetworkProxy> network_proxy(
      new MockSupplicantNetworkProxy());
  EXPECT_CALL(*proxy_factory(),
              CreateSupplicantNetworkProxy(kPath, WPASupplicant::kDBusAddr))
      .WillOnce(ReturnAndReleasePointee(&network_proxy));
  EXPECT_CALL(*network_proxy, SetEnabled(false));
  EXPECT_CALL(*eap_state_handler_, Reset());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(kPath)).Times(0);
  EXPECT_CALL(*service, SetFailure(_)).Times(0);
  EXPECT_CALL(*service, SetState(Service::kStateIdle)).Times(AtLeast(1));
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
  EXPECT_EQ(NULL, GetCurrentService().get());
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
}

TEST_F(WiFiMainTest, DisconnectCurrentServiceWithFailure) {
  StartWiFi();
  ::DBus::Path kPath("/fake/path");
  MockWiFiServiceRefPtr service(SetupConnectedService(kPath, NULL, NULL));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  InitiateDisconnect(service);

  // |current_service_| should not change until supplicant reports
  // a BSS change.
  EXPECT_EQ(service, GetCurrentService());

  // Expect that the entry associated with this network will be disabled.
  scoped_ptr<MockSupplicantNetworkProxy> network_proxy(
      new MockSupplicantNetworkProxy());
  EXPECT_CALL(*proxy_factory(),
              CreateSupplicantNetworkProxy(kPath, WPASupplicant::kDBusAddr))
      .WillOnce(ReturnAndReleasePointee(&network_proxy));
  EXPECT_CALL(*network_proxy, SetEnabled(false));
  EXPECT_CALL(*eap_state_handler_, Reset());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(kPath)).Times(0);
  EXPECT_CALL(*service, SetFailure(Service::kFailureOutOfRange));
  EXPECT_CALL(*service, SetState(Service::kStateIdle)).Times(AtLeast(1));
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
  EXPECT_EQ(NULL, GetSelectedService().get());
}

TEST_F(WiFiMainTest, DisconnectCurrentServiceWithPending) {
  StartWiFi();
  MockWiFiServiceRefPtr service0(SetupConnectedService(DBus::Path(),
                                                       NULL, NULL));
  MockWiFiServiceRefPtr service1(SetupConnectingService(DBus::Path(),
                                                        NULL, NULL));
  EXPECT_EQ(service0, GetCurrentService());
  EXPECT_EQ(service1, GetPendingService());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect()).Times(0);
  InitiateDisconnect(service0);

  EXPECT_EQ(service0, GetCurrentService());
  EXPECT_EQ(service1, GetPendingService());
  EXPECT_FALSE(GetPendingTimeout().IsCancelled());

  EXPECT_CALL(*service0, SetState(Service::kStateIdle)).Times(AtLeast(1));
  EXPECT_CALL(*service0, SetFailure(_)).Times(0);
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
}

TEST_F(WiFiMainTest, DisconnectCurrentServiceWhileRoaming) {
  StartWiFi();
  DBus::Path kPath("/fake/path");
  WiFiServiceRefPtr service(SetupConnectedService(kPath, NULL, NULL));

  // As it roams to another AP, supplicant signals that it is in
  // the authenticating state.
  ReportStateChanged(WPASupplicant::kInterfaceStateAuthenticating);

  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), RemoveNetwork(kPath));
  InitiateDisconnect(service);

  // Because the interface was not connected, we should have immediately
  // forced ourselves into a disconnected state.
  EXPECT_EQ(NULL, GetCurrentService().get());
  EXPECT_EQ(NULL, GetSelectedService().get());

  // Check calls before TearDown/dtor.
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
}

TEST_F(WiFiMainTest, DisconnectWithWiFiServiceConnected) {
  StartWiFi();
  MockWiFiServiceRefPtr service0(SetupConnectedService(DBus::Path(),
                                                       NULL, NULL));
  NiceScopedMockLog log;
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(2);
  EXPECT_CALL(log, Log(_, _,
                       HasSubstr("DisconnectFromIfActive service"))).Times(1);
  EXPECT_CALL(log, Log(_, _, HasSubstr("DisconnectFrom service"))).Times(1);
  EXPECT_CALL(*service0, IsActive(_)).Times(0);
  InitiateDisconnectIfActive(service0);

  Mock::VerifyAndClearExpectations(&log);
  Mock::VerifyAndClearExpectations(service0);
  ScopeLogger::GetInstance()->set_verbose_level(0);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
}

TEST_F(WiFiMainTest, DisconnectWithWiFiServiceIdle) {
  StartWiFi();
  MockWiFiServiceRefPtr service0(SetupConnectedService(DBus::Path(),
                                                       NULL, NULL));
  InitiateDisconnectIfActive(service0);
  MockWiFiServiceRefPtr service1(SetupConnectedService(DBus::Path(),
                                                       NULL, NULL));
  NiceScopedMockLog log;
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(2);
  EXPECT_CALL(log, Log(_, _,
                       HasSubstr("DisconnectFromIfActive service"))).Times(1);
  EXPECT_CALL(*service0, IsActive(_)).WillOnce(Return(false));
  EXPECT_CALL(log, Log(_, _, HasSubstr("is not active, no need"))).Times(1);
  EXPECT_CALL(log, Log(logging::LOG_WARNING, _,
                       HasSubstr("In DisconnectFrom():"))).Times(0);
  InitiateDisconnectIfActive(service0);

  Mock::VerifyAndClearExpectations(&log);
  Mock::VerifyAndClearExpectations(service0);
  ScopeLogger::GetInstance()->set_verbose_level(0);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
}

TEST_F(WiFiMainTest, DisconnectWithWiFiServiceConnectedInError) {
  StartWiFi();
  MockWiFiServiceRefPtr service0(SetupConnectedService(DBus::Path(),
                                                       NULL, NULL));
  SetCurrentService(NULL);
  ResetPendingService();
  NiceScopedMockLog log;
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(2);
  EXPECT_CALL(log, Log(_, _,
                       HasSubstr("DisconnectFromIfActive service"))).Times(1);
  EXPECT_CALL(*service0, IsActive(_)).WillOnce(Return(true));
  EXPECT_CALL(log, Log(_, _, HasSubstr("DisconnectFrom service"))).Times(1);
  EXPECT_CALL(log, Log(logging::LOG_WARNING, _,
                       HasSubstr("In DisconnectFrom():"))).Times(1);
  InitiateDisconnectIfActive(service0);

  Mock::VerifyAndClearExpectations(&log);
  Mock::VerifyAndClearExpectations(service0);
  ScopeLogger::GetInstance()->set_verbose_level(0);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
}

TEST_F(WiFiMainTest, TimeoutPendingServiceWithEndpoints) {
  StartScan(WiFi::kScanMethodProgressive);
  const base::CancelableClosure &pending_timeout = GetPendingTimeout();
  EXPECT_TRUE(pending_timeout.IsCancelled());
  MockWiFiServiceRefPtr service = AttemptConnection(
      WiFi::kScanMethodProgressive, nullptr, nullptr);

  // Timeout the connection attempt.
  EXPECT_FALSE(pending_timeout.IsCancelled());
  EXPECT_EQ(service, GetPendingService());
  // Simulate a service with a wifi_ reference calling DisconnectFrom().
  EXPECT_CALL(*service, DisconnectWithFailure(Service::kFailureOutOfRange,
                                              _,
                                              StrEq("PendingTimeoutHandler")))
      .WillOnce(InvokeWithoutArgs(this, &WiFiObjectTest::ResetPendingService));
  EXPECT_CALL(*service, HasEndpoints()).Times(0);
  // DisconnectFrom() should not be called directly from WiFi.
  EXPECT_CALL(*service, SetState(Service::kStateIdle)).Times(1);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect()).Times(0);

  // Innocuous redundant call to NotifyDeviceScanFinished.
  ExpectFoundNothing();
  EXPECT_CALL(*metrics(), NotifyDeviceConnectFinished(_)).Times(0);
  NiceScopedMockLog log;
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(10);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _,
                       HasSubstr("-> PROGRESSIVE_FINISHED_NOCONNECTION")));
  pending_timeout.callback().Run();
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);
  // Service state should be idle, so it is connectable again.
  EXPECT_EQ(Service::kStateIdle, service->state());
  Mock::VerifyAndClearExpectations(service);

  ScopeLogger::GetInstance()->set_verbose_level(0);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
}

TEST_F(WiFiMainTest, TimeoutPendingServiceWithoutEndpoints) {
  StartWiFi();
  const base::CancelableClosure &pending_timeout = GetPendingTimeout();
  EXPECT_TRUE(pending_timeout.IsCancelled());
  MockWiFiServiceRefPtr service(
      SetupConnectingService(DBus::Path(), NULL, NULL));
  EXPECT_FALSE(pending_timeout.IsCancelled());
  EXPECT_EQ(service, GetPendingService());
  // We expect the service to get a disconnect call, but in this scenario
  // the service does nothing.
  EXPECT_CALL(*service, DisconnectWithFailure(Service::kFailureOutOfRange,
                                              _,
                                              StrEq("PendingTimeoutHandler")));
  EXPECT_CALL(*service, HasEndpoints()).WillOnce(Return(false));
  // DisconnectFrom() should be called directly from WiFi.
  EXPECT_CALL(*service, SetState(Service::kStateIdle)).Times(AtLeast(1));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  pending_timeout.callback().Run();
  EXPECT_EQ(NULL, GetPendingService().get());
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

MATCHER_P(HasHiddenSSID_FullScan, ssid, "") {
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

MATCHER(HasNoHiddenSSID_FullScan, "") {
  map<string, DBus::Variant>::const_iterator it =
      arg.find(WPASupplicant::kPropertyScanSSIDs);
  return it == arg.end();
}

TEST_F(WiFiMainTest, ScanHidden_FullScan) {
  EnableFullScan();
  vector<uint8_t>kSSID(1, 'a');
  ByteArrays ssids;
  ssids.push_back(kSSID);

  StartWiFi();
  EXPECT_CALL(*wifi_provider(), GetHiddenSSIDList()).WillOnce(Return(ssids));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(),
              Scan(HasHiddenSSID_FullScan(kSSID)));
  dispatcher_.DispatchPendingEvents();
}

// This test is slightly different from the test in scan_session_unittest.cc
// because this tests the piece of WiFi that builds the SSID list.
TEST_F(WiFiMainTest, ScanHidden) {
  // Clear the Mock ScanSession because hidden SSIDs are added when wifi
  // instantiates a new ScanSession (and it won't instantiate a new ScanSession
  // if there's already one there).
  ClearScanSession();
  vector<uint8_t>kSSID(1, 'a');
  ByteArrays ssids;
  ssids.push_back(kSSID);

  EXPECT_CALL(*wifi_provider(), GetHiddenSSIDList()).WillOnce(Return(ssids));
  StartWiFi();
  EXPECT_CALL(netlink_manager_,
              SendNl80211Message(HasHiddenSSID(kNl80211FamilyId), _, _, _));
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, ScanNoHidden_FullScan) {
  EnableFullScan();
  StartWiFi();
  EXPECT_CALL(*wifi_provider(), GetHiddenSSIDList())
      .WillOnce(Return(ByteArrays()));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(HasNoHiddenSSID_FullScan()));
  dispatcher_.DispatchPendingEvents();
}

// This test is slightly different from the test in scan_session_unittest.cc
// because this tests the piece of WiFi that builds the SSID list.
TEST_F(WiFiMainTest, ScanNoHidden) {
  // Clear the Mock ScanSession because hidden SSIDs are added when wifi
  // instantiates a new ScanSession (and it won't instantiate a new ScanSession
  // if there's already one there).
  ClearScanSession();
  EXPECT_CALL(*wifi_provider(), GetHiddenSSIDList())
      .WillOnce(Return(ByteArrays()));
  StartWiFi();
  EXPECT_CALL(netlink_manager_,
              SendNl80211Message(HasNoHiddenSSID(kNl80211FamilyId), _, _, _));
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, ScanWiFiDisabledAfterResume) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, EndsWith(
      "Ignoring scan request while device is not enabled."))).Times(1);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  EXPECT_CALL(*scan_session_, InitiateScan()).Times(0);
  StartWiFi();
  StopWiFi();
  // A scan is queued when WiFi resumes.
  OnAfterResume();
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, ScanRejected) {
  StartWiFi();
  ReportScanDone();
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);

  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_))
      .WillOnce(Throw(DBus::Error("don't care", "don't care")));
  TriggerScan(WiFi::kScanMethodFull);
  dispatcher_.DispatchPendingEvents();
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);
}

TEST_F(WiFiMainTest, ProgressiveScanFound) {
  // Set min & max scan frequency count to 1 so each scan will be of a single
  // frequency.
  SetScanSize(1, 1);

  // Do the first scan (finds nothing).
  StartScan(WiFi::kScanMethodProgressive);
  EXPECT_CALL(*manager(), OnDeviceGeolocationInfoUpdated(_)).Times(0);
  ReportScanDoneKeepScanSession();

  // Do the second scan (connects afterwards).
  ExpectScanStart(WiFi::kScanMethodProgressive, true);
  dispatcher_.DispatchPendingEvents();
  VerifyScanState(WiFi::kScanScanning, WiFi::kScanMethodProgressive);
  ReportScanDoneKeepScanSession();

  // Connect after second scan.
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
  EXPECT_CALL(*metrics(), NotifyDeviceScanFinished(_));
  EXPECT_CALL(*scan_session_, InitiateScan()).Times(0);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  EXPECT_CALL(*adaptor_, EmitBoolChanged(kScanningProperty, false));
  SetPendingService(service);

  // Verify that the third scan aborts and there is no further scan.
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, EndsWith(
      "Ignoring scan request while connecting to an AP."))).Times(1);
  dispatcher_.DispatchPendingEvents();
  VerifyScanState(WiFi::kScanConnecting, WiFi::kScanMethodProgressive);
}

TEST_F(WiFiMainTest, ProgressiveScanNotFound) {
  // Set min & max scan frequency count to 1 so each scan will be of a single
  // frequency.
  SetScanSize(1, 1);

  // This test never connects
  EXPECT_CALL(*metrics(), NotifyDeviceConnectStarted(_, _)).Times(0);
  EXPECT_CALL(*metrics(), NotifyDeviceConnectFinished(_)).Times(0);

  // Do the first scan (finds nothing).
  StartScan(WiFi::kScanMethodProgressive);
  ReportScanDoneKeepScanSession();

  // Do the second scan (finds nothing).
  ExpectScanStart(WiFi::kScanMethodProgressive, true);
  EXPECT_CALL(*manager(), OnDeviceGeolocationInfoUpdated(_)).Times(0);
  dispatcher_.DispatchPendingEvents();
  VerifyScanState(WiFi::kScanScanning, WiFi::kScanMethodProgressive);
  ReportScanDoneKeepScanSession();

  // Do the third scan. After (simulated) exhausting of search frequencies,
  // verify that this scan uses supplicant rather than internal (progressive)
  // scan.
  EXPECT_CALL(*scan_session_, HasMoreFrequencies()).WillOnce(Return(false));
  EXPECT_CALL(*scan_session_, InitiateScan()).Times(0);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
  dispatcher_.DispatchPendingEvents();
  VerifyScanState(WiFi::kScanScanning,
                  WiFi::kScanMethodProgressiveFinishedToFull);

  // And verify that ScanDone reports a complete scan (i.e., the
  // wifi_::scan_session_ has truly been cleared).
  ExpectScanStop();
  ExpectFoundNothing();
  ReportScanDoneKeepScanSession();
  dispatcher_.DispatchPendingEvents();  // Launch UpdateScanStateAfterScanDone
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);
}

TEST_F(WiFiMainTest, ProgressiveScanError) {
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);
  ClearScanSession();  // Clear Mock ScanSession to get an actual ScanSession.
  StartWiFi();  // Posts |ProgressiveScanTask|.

  EXPECT_CALL(netlink_manager_, SendNl80211Message(
      IsNl80211Command(kNl80211FamilyId, NL80211_CMD_TRIGGER_SCAN), _, _, _));
  dispatcher_.DispatchPendingEvents();  // Executes |ProgressiveScanTask|.

  // Calls |WiFi::OnFailedProgressiveScan| which calls |ScanTask|
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(1);
  NewScanResultsMessage not_supposed_to_get_this_message;
  OnTriggerScanResponse(not_supposed_to_get_this_message);
  VerifyScanState(WiFi::kScanScanning, WiFi::kScanMethodProgressiveErrorToFull);

  EXPECT_TRUE(IsScanSessionNull());

  // Post and execute |UpdateScanStateAfterScanDone|.
  ReportScanDoneKeepScanSession();
  dispatcher_.DispatchPendingEvents();
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);
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
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
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
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
  EXPECT_CALL(*service, SetState(Service::kStateAssociating));
  EXPECT_CALL(*service, SetState(Service::kStateConfiguring));
  EXPECT_CALL(*service, ResetSuspectedCredentialFailures());
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
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
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
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
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
  // level of supplicant debugging.
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

  EXPECT_CALL(*service, SetState(Service::kStateIdle)).Times(AtLeast(1));
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
  EXPECT_EQ(NULL, GetCurrentService().get());
  EXPECT_EQ(NULL, GetPendingService().get());
  EXPECT_FALSE(GetIsRoamingInProgress());
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
  EXPECT_CALL(*service1, ResetSuspectedCredentialFailures());
  EXPECT_CALL(*wifi_provider(), IncrementConnectCount(_));
  ReportStateChanged(WPASupplicant::kInterfaceStateCompleted);
  EXPECT_EQ(service1.get(), GetCurrentService().get());
  EXPECT_FALSE(GetIsRoamingInProgress());
  Mock::VerifyAndClearExpectations(service0);
  Mock::VerifyAndClearExpectations(service1);
}

TEST_F(WiFiMainTest, CurrentBSSChangedUpdateServiceEndpoint) {
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  VerifyScanState(WiFi::kScanScanning, WiFi::kScanMethodProgressive);

  MockWiFiServiceRefPtr service =
      SetupConnectedService(DBus::Path(), NULL, NULL);
  WiFiEndpointRefPtr endpoint;
  ::DBus::Path bss_path =
      AddEndpointToService(service, 0, 0, kNetworkModeAdHoc, &endpoint);
  EXPECT_CALL(*service, NotifyCurrentEndpoint(EndpointMatch(endpoint)));
  ReportCurrentBSSChanged(bss_path);
  EXPECT_TRUE(GetIsRoamingInProgress());
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);

  // If we report a "completed" state change on a connected service after
  // wpa_supplicant has roamed, we should renew our IPConfig.
  scoped_refptr<MockIPConfig> ipconfig(
      new MockIPConfig(control_interface(), kDeviceName));
  SetIPConfig(ipconfig);
  EXPECT_CALL(*service, IsConnected()).WillOnce(Return(true));
  EXPECT_CALL(*ipconfig, RenewIP());
  ReportStateChanged(WPASupplicant::kInterfaceStateCompleted);
  Mock::VerifyAndClearExpectations(ipconfig);
  EXPECT_FALSE(GetIsRoamingInProgress());
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

TEST_F(WiFiMainTest, ConnectedToUnintendedPreemptsPending) {
  StartWiFi();
  ::DBus::Path bss_path;
  // Connecting two different services back-to-back.
  MockWiFiServiceRefPtr unintended_service(
      SetupConnectingService(DBus::Path(), NULL, &bss_path));
  MockWiFiServiceRefPtr intended_service(
      SetupConnectingService(DBus::Path(), NULL, NULL));

  // Verify the pending service.
  EXPECT_EQ(intended_service.get(), GetPendingService().get());

  // Connected to the unintended service (service0).
  ReportCurrentBSSChanged(bss_path);

  // Verify the pending service is disconnected, and the service state is back
  // to idle, so it is connectable again.
  EXPECT_EQ(NULL, GetPendingService().get());
  EXPECT_EQ(NULL, GetCurrentService().get());
  EXPECT_EQ(Service::kStateIdle, intended_service->state());
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
      ContainsKey(arg, WPASupplicant::kNetworkPropertyDisableVHT) &&
      ContainsKey(arg, WPASupplicant::kNetworkPropertyBgscan) == bgscan;
}

TEST_F(WiFiMainTest, AddNetworkArgs) {
  StartWiFi();
  MockWiFiServiceRefPtr service;
  MakeNewEndpointAndService(0, 0, kNetworkModeAdHoc, NULL, &service);
  EXPECT_CALL(*service, GetSupplicantConfigurationParameters());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), AddNetwork(WiFiAddedArgs(true)));
  EXPECT_TRUE(SetBgscanMethod(WPASupplicant::kNetworkBgscanMethodSimple));
  InitiateConnect(service);
}

TEST_F(WiFiMainTest, AddNetworkArgsNoBgscan) {
  StartWiFi();
  MockWiFiServiceRefPtr service;
  MakeNewEndpointAndService(0, 0, kNetworkModeAdHoc, NULL, &service);
  EXPECT_CALL(*service, GetSupplicantConfigurationParameters());
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), AddNetwork(WiFiAddedArgs(false)));
  InitiateConnect(service);
}

TEST_F(WiFiMainTest, AppendBgscan) {
  StartWiFi();
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
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

TEST_F(WiFiMainTest, ScanTimerIdle_FullScan) {
  EnableFullScan();
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  ReportScanDone();
  CancelScanTimer();
  EXPECT_TRUE(GetScanTimer().IsCancelled());

  EXPECT_CALL(*manager(), OnDeviceGeolocationInfoUpdated(_));
  dispatcher_.DispatchPendingEvents();
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_));
  FireScanTimer();
  dispatcher_.DispatchPendingEvents();
  EXPECT_FALSE(GetScanTimer().IsCancelled());  // Automatically re-armed.
}

TEST_F(WiFiMainTest, ScanTimerIdle) {
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  ReportScanDone();
  CancelScanTimer();
  EXPECT_TRUE(GetScanTimer().IsCancelled());
  dispatcher_.DispatchPendingEvents();
  InstallMockScanSession();
  EXPECT_CALL(*scan_session_, InitiateScan());
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
  EXPECT_CALL(*scan_session_, InitiateScan()).Times(0);
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
  EXPECT_CALL(*scan_session_, InitiateScan()).Times(0);
  FireScanTimer();
  dispatcher_.DispatchPendingEvents();
  EXPECT_FALSE(GetScanTimer().IsCancelled());  // Automatically re-armed.
}

TEST_F(WiFiMainTest, ScanTimerReconfigured) {
  StartWiFi();
  CancelScanTimer();
  EXPECT_TRUE(GetScanTimer().IsCancelled());

  SetScanInterval(1, NULL);
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

  SetScanInterval(0, NULL);
  EXPECT_TRUE(GetScanTimer().IsCancelled());
}

TEST_F(WiFiMainTest, ScanOnDisconnectWithHidden_FullScan) {
  EnableFullScan();
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  SetupConnectedService(DBus::Path(), NULL, NULL);
  vector<uint8_t>kSSID(1, 'a');
  ByteArrays ssids;
  ssids.push_back(kSSID);
  ExpectScanIdle();
  EXPECT_CALL(*wifi_provider(), GetHiddenSSIDList())
      .WillRepeatedly(Return(ssids));
  EXPECT_CALL(*GetSupplicantInterfaceProxy(),
              Scan(HasHiddenSSID_FullScan(kSSID)));
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, ScanOnDisconnectWithHidden) {
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  ReportScanDone();
  SetupConnectedService(DBus::Path(), NULL, NULL);
  InstallMockScanSession();
  vector<uint8_t>kSSID(1, 'a');
  ByteArrays ssids;
  ssids.push_back(kSSID);
  ExpectScanIdle();
  EXPECT_CALL(*wifi_provider(), GetHiddenSSIDList())
      .WillRepeatedly(Return(ssids));
  EXPECT_CALL(*scan_session_, InitiateScan());
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, NoScanOnDisconnectWithoutHidden) {
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  SetupConnectedService(DBus::Path(), NULL, NULL);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  EXPECT_TRUE(IsScanSessionNull());
  EXPECT_CALL(*wifi_provider(), GetHiddenSSIDList())
      .WillRepeatedly(Return(ByteArrays()));
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
  dispatcher_.DispatchPendingEvents();
}

TEST_F(WiFiMainTest, LinkMonitorFailure) {
  ScopedMockLog log;
  auto link_monitor = new StrictMock<MockLinkMonitor>();
  StartWiFi();
  SetLinkMonitor(link_monitor);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*link_monitor, IsGatewayFound())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));

  // We never had an ARP reply during this connection, so we assume
  // the problem is gateway, rather than link.
  EXPECT_CALL(log, Log(logging::LOG_INFO, _,
                       EndsWith("gateway was never found."))).Times(1);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Reattach()).Times(0);
  OnLinkMonitorFailure();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());

  // No supplicant, so we can't Reattach.
  OnSupplicantVanish();
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                       EndsWith("Cannot reassociate."))).Times(1);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Reattach()).Times(0);
  OnLinkMonitorFailure();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());

  // Normal case: call Reattach.
  OnSupplicantAppear();
  EXPECT_CALL(log, Log(logging::LOG_INFO, _,
                       EndsWith("Called Reattach()."))).Times(1);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Reattach()).Times(1);
  OnLinkMonitorFailure();
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
}

TEST_F(WiFiMainTest, SuspectCredentialsOpen) {
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
  EXPECT_CALL(*service, AddSuspectedCredentialFailure()).Times(0);
  EXPECT_FALSE(SuspectCredentials(service, NULL));
}

TEST_F(WiFiMainTest, SuspectCredentialsWPA) {
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityWpa);
  ReportStateChanged(WPASupplicant::kInterfaceState4WayHandshake);
  EXPECT_CALL(*service, AddSuspectedCredentialFailure())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_FALSE(SuspectCredentials(service, NULL));
  Service::ConnectFailure failure;
  EXPECT_TRUE(SuspectCredentials(service, &failure));
  EXPECT_EQ(Service::kFailureBadPassphrase, failure);
}

TEST_F(WiFiMainTest, SuspectCredentialsWEP) {
  StartWiFi();
  dispatcher_.DispatchPendingEvents();
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityWep);
  ExpectConnecting();
  InitiateConnect(service);
  SetCurrentService(service);

  // These expectations are very much like SetupConnectedService except
  // that we verify that ResetSupsectCredentialFailures() is not called
  // on the service just because supplicant entered the Completed state.
  EXPECT_CALL(*service, SetState(Service::kStateConfiguring));
  EXPECT_CALL(*service, ResetSuspectedCredentialFailures()).Times(0);
  EXPECT_CALL(*dhcp_provider(), CreateConfig(_, _, _, _)).Times(AnyNumber());
  EXPECT_CALL(*dhcp_config_.get(), RequestIP()).Times(AnyNumber());
  EXPECT_CALL(*manager(), device_info()).WillRepeatedly(Return(device_info()));
  EXPECT_CALL(*device_info(), GetByteCounts(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(0LL), Return(true)));
  ReportStateChanged(WPASupplicant::kInterfaceStateCompleted);

  Mock::VerifyAndClearExpectations(device_info());
  Mock::VerifyAndClearExpectations(service);

  // Successful connect.
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), EnableHighBitrates()).Times(1);
  EXPECT_CALL(*service, ResetSuspectedCredentialFailures());
  ReportConnected();

  EXPECT_CALL(*device_info(), GetByteCounts(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(1LL), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(0LL), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(0LL), Return(true)));

  // If there was an increased byte-count while we were timing out DHCP,
  // this should be considered a DHCP failure and not a credential failure.
  EXPECT_CALL(*service, ResetSuspectedCredentialFailures()).Times(0);
  EXPECT_CALL(*service, DisconnectWithFailure(Service::kFailureDHCP,
                                              _,
                                              StrEq("OnIPConfigFailure")));
  ReportIPConfigFailure();
  Mock::VerifyAndClearExpectations(service);

  // Connection failed during DHCP but service does not (yet) believe this is
  // due to a passphrase issue.
  EXPECT_CALL(*service, AddSuspectedCredentialFailure())
      .WillOnce(Return(false));
  EXPECT_CALL(*service, DisconnectWithFailure(Service::kFailureDHCP,
                                              _,
                                              StrEq("OnIPConfigFailure")));
  ReportIPConfigFailure();
  Mock::VerifyAndClearExpectations(service);

  // Connection failed during DHCP and service believes this is due to a
  // passphrase issue.
  EXPECT_CALL(*service, AddSuspectedCredentialFailure())
      .WillOnce(Return(true));
  EXPECT_CALL(*service,
              DisconnectWithFailure(Service::kFailureBadPassphrase,
                                    _,
                                    StrEq("OnIPConfigFailure")));
  ReportIPConfigFailure();
}

TEST_F(WiFiMainTest, SuspectCredentialsEAPInProgress) {
  MockWiFiServiceRefPtr service = MakeMockService(kSecurity8021x);
  EXPECT_CALL(*eap_state_handler_, is_eap_in_progress())
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*service, AddSuspectedCredentialFailure()).Times(0);
  EXPECT_FALSE(SuspectCredentials(service, NULL));
  Mock::VerifyAndClearExpectations(service);

  EXPECT_CALL(*service, AddSuspectedCredentialFailure()).WillOnce(Return(true));
  Service::ConnectFailure failure;
  EXPECT_TRUE(SuspectCredentials(service, &failure));
  EXPECT_EQ(Service::kFailureEAPAuthentication, failure);
  Mock::VerifyAndClearExpectations(service);

  EXPECT_CALL(*service, AddSuspectedCredentialFailure()).Times(0);
  EXPECT_FALSE(SuspectCredentials(service, NULL));
  Mock::VerifyAndClearExpectations(service);

  EXPECT_CALL(*service, AddSuspectedCredentialFailure())
      .WillOnce(Return(false));
  EXPECT_FALSE(SuspectCredentials(service, NULL));
}

TEST_F(WiFiMainTest, SuspectCredentialsYieldFailureWPA) {
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityWpa);
  SetPendingService(service);
  ReportStateChanged(WPASupplicant::kInterfaceState4WayHandshake);

  ExpectScanIdle();
  EXPECT_CALL(*service, AddSuspectedCredentialFailure()).WillOnce(Return(true));
  EXPECT_CALL(*service, SetFailure(Service::kFailureBadPassphrase));
  EXPECT_CALL(*service, SetState(Service::kStateIdle));
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _, EndsWith(kErrorBadPassphrase)));
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
}

TEST_F(WiFiMainTest, SuspectCredentialsYieldFailureEAP) {
  MockWiFiServiceRefPtr service = MakeMockService(kSecurity8021x);
  SetCurrentService(service);

  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*service, SetState(Service::kStateIdle));
  // Ensure that we retrieve is_eap_in_progress() before resetting the
  // EAP handler's state.
  InSequence seq;
  EXPECT_CALL(*eap_state_handler_, is_eap_in_progress())
      .WillOnce(Return(true));
  EXPECT_CALL(*service, AddSuspectedCredentialFailure()).WillOnce(Return(true));
  EXPECT_CALL(*service, SetFailure(Service::kFailureEAPAuthentication));
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                       EndsWith(kErrorEapAuthenticationFailed)));
  EXPECT_CALL(*eap_state_handler_, Reset());
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

TEST_F(WiFiTimerTest, RequestStationInfo) {
  EXPECT_CALL(mock_dispatcher_, PostTask(_)).Times(AnyNumber());
  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(_, _)).Times(AnyNumber());

  // Setup a connected service here while we have the expectations above set.
  StartWiFi();
  MockWiFiServiceRefPtr service =
      SetupConnectedService(DBus::Path(), NULL, NULL);
  string connected_bss = GetSupplicantBSS();
  Mock::VerifyAndClearExpectations(&mock_dispatcher_);

  EXPECT_CALL(netlink_manager_, SendNl80211Message(_, _, _, _)).Times(0);
  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(_, _)).Times(0);
  NiceScopedMockLog log;

  // There is no current_service_.
  EXPECT_CALL(log, Log(_, _, HasSubstr("we are not connected")));
  SetCurrentService(NULL);
  RequestStationInfo();

  // current_service_ is not connected.
  EXPECT_CALL(*service, IsConnected()).WillOnce(Return(false));
  SetCurrentService(service);
  EXPECT_CALL(log, Log(_, _, HasSubstr("we are not connected")));
  RequestStationInfo();

  // Endpoint does not exist in endpoint_by_rpcid_.
  EXPECT_CALL(*service, IsConnected()).WillRepeatedly(Return(true));
  SetSupplicantBSS("/some/path/that/does/not/exist/in/endpoint_by_rpcid");
  EXPECT_CALL(log, Log(_, _, HasSubstr(
      "Can't get endpoint for current supplicant BSS")));
  RequestStationInfo();
  Mock::VerifyAndClearExpectations(&netlink_manager_);
  Mock::VerifyAndClearExpectations(&mock_dispatcher_);

  // We successfully trigger a request to get the station and start a timer
  // for the next call.
  EXPECT_CALL(netlink_manager_, SendNl80211Message(
      IsNl80211Command(kNl80211FamilyId, NL80211_CMD_GET_STATION), _, _, _));
  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(
      _, WiFi::kRequestStationInfoPeriodSeconds * 1000));
  SetSupplicantBSS(connected_bss);
  RequestStationInfo();

  // Now test that a properly formatted New Station message updates strength.
  NewStationMessage new_station;
  new_station.attributes()->CreateRawAttribute(NL80211_ATTR_MAC, "BSSID");

  // Confirm that up until now no link statistics exist.
  KeyValueStore link_statistics = GetLinkStatistics();
  EXPECT_TRUE(link_statistics.IsEmpty());

  // Use a reference to the endpoint instance in the WiFi device instead of
  // the copy returned by SetupConnectedService().
  WiFiEndpointRefPtr endpoint = GetEndpointMap().begin()->second;
  new_station.attributes()->SetRawAttributeValue(
      NL80211_ATTR_MAC, ByteString::CreateFromHexString(endpoint->bssid_hex()));
  new_station.attributes()->CreateNestedAttribute(
      NL80211_ATTR_STA_INFO, "Station Info");
  AttributeListRefPtr station_info;
  new_station.attributes()->GetNestedAttributeList(
      NL80211_ATTR_STA_INFO, &station_info);
  station_info->CreateU8Attribute(NL80211_STA_INFO_SIGNAL, "Signal");
  const int kSignalValue = -20;
  station_info->SetU8AttributeValue(NL80211_STA_INFO_SIGNAL, kSignalValue);
  station_info->CreateU8Attribute(NL80211_STA_INFO_SIGNAL_AVG, "SignalAverage");
  const int kSignalAvgValue = -40;
  station_info->SetU8AttributeValue(NL80211_STA_INFO_SIGNAL_AVG,
                                    kSignalAvgValue);
  station_info->CreateU32Attribute(NL80211_STA_INFO_INACTIVE_TIME,
                                   "InactiveTime");
  const int32_t kInactiveTime = 100;
  station_info->SetU32AttributeValue(NL80211_STA_INFO_INACTIVE_TIME,
                                     kInactiveTime);
  station_info->CreateU32Attribute(NL80211_STA_INFO_RX_PACKETS,
                                   "ReceivedSuccesses");
  const int32_t kReceiveSuccesses = 200;
  station_info->SetU32AttributeValue(NL80211_STA_INFO_RX_PACKETS,
                                     kReceiveSuccesses);
  station_info->CreateU32Attribute(NL80211_STA_INFO_TX_FAILED,
                                   "TransmitFailed");
  const int32_t kTransmitFailed = 300;
  station_info->SetU32AttributeValue(NL80211_STA_INFO_TX_FAILED,
                                     kTransmitFailed);
  station_info->CreateU32Attribute(NL80211_STA_INFO_TX_PACKETS,
                                   "TransmitSuccesses");
  const int32_t kTransmitSuccesses = 400;
  station_info->SetU32AttributeValue(NL80211_STA_INFO_TX_PACKETS,
                                     kTransmitSuccesses);
  station_info->CreateU32Attribute(NL80211_STA_INFO_TX_RETRIES,
                                   "TransmitRetries");
  const int32_t kTransmitRetries = 500;
  station_info->SetU32AttributeValue(NL80211_STA_INFO_TX_RETRIES,
                                     kTransmitRetries);
  station_info->CreateNestedAttribute(NL80211_STA_INFO_TX_BITRATE,
                                      "Bitrate Info");

  // Embed transmit bitrate info within the station info element.
  AttributeListRefPtr bitrate_info;
  station_info->GetNestedAttributeList(
      NL80211_STA_INFO_TX_BITRATE, &bitrate_info);
  bitrate_info->CreateU16Attribute(NL80211_RATE_INFO_BITRATE, "Bitrate");
  const int16_t kBitrate = 6005;
  bitrate_info->SetU16AttributeValue(NL80211_RATE_INFO_BITRATE, kBitrate);
  bitrate_info->CreateU8Attribute(NL80211_RATE_INFO_MCS, "MCS");
  const int16_t kMCS = 7;
  bitrate_info->SetU8AttributeValue(NL80211_RATE_INFO_MCS, kMCS);
  bitrate_info->CreateFlagAttribute(NL80211_RATE_INFO_40_MHZ_WIDTH, "HT40");
  bitrate_info->SetFlagAttributeValue(NL80211_RATE_INFO_40_MHZ_WIDTH, true);
  bitrate_info->CreateFlagAttribute(NL80211_RATE_INFO_SHORT_GI, "SGI");
  bitrate_info->SetFlagAttributeValue(NL80211_RATE_INFO_SHORT_GI, false);
  station_info->SetNestedAttributeHasAValue(NL80211_STA_INFO_TX_BITRATE);

  new_station.attributes()->SetNestedAttributeHasAValue(NL80211_ATTR_STA_INFO);

  EXPECT_NE(kSignalValue, endpoint->signal_strength());
  EXPECT_CALL(*wifi_provider(), OnEndpointUpdated(EndpointMatch(endpoint)));
  EXPECT_CALL(*metrics(), NotifyWifiTxBitrate(kBitrate/10));
  AttributeListConstRefPtr station_info_prime;
  ReportReceivedStationInfo(new_station);
  EXPECT_EQ(kSignalValue, endpoint->signal_strength());

  link_statistics = GetLinkStatistics();
  ASSERT_FALSE(link_statistics.IsEmpty());
  ASSERT_TRUE(link_statistics.ContainsInt(kLastReceiveSignalDbmProperty));
  EXPECT_EQ(kSignalValue,
            link_statistics.GetInt(kLastReceiveSignalDbmProperty));
  ASSERT_TRUE(link_statistics.ContainsInt(kAverageReceiveSignalDbmProperty));
  EXPECT_EQ(kSignalAvgValue,
            link_statistics.GetInt(kAverageReceiveSignalDbmProperty));
  ASSERT_TRUE(link_statistics.ContainsUint(kInactiveTimeMillisecondsProperty));
  EXPECT_EQ(kInactiveTime,
            link_statistics.GetUint(kInactiveTimeMillisecondsProperty));
  ASSERT_TRUE(link_statistics.ContainsUint(kPacketReceiveSuccessesProperty));
  EXPECT_EQ(kReceiveSuccesses,
            link_statistics.GetUint(kPacketReceiveSuccessesProperty));
  ASSERT_TRUE(link_statistics.ContainsUint(kPacketTransmitFailuresProperty));
  EXPECT_EQ(kTransmitFailed,
            link_statistics.GetUint(kPacketTransmitFailuresProperty));
  ASSERT_TRUE(link_statistics.ContainsUint(kPacketTransmitSuccessesProperty));
  EXPECT_EQ(kTransmitSuccesses,
            link_statistics.GetUint(kPacketTransmitSuccessesProperty));
  ASSERT_TRUE(link_statistics.ContainsUint(kTransmitRetriesProperty));
  EXPECT_EQ(kTransmitRetries,
            link_statistics.GetUint(kTransmitRetriesProperty));
  EXPECT_EQ(StringPrintf("%d.%d MBit/s MCS %d 40MHz",
                         kBitrate / 10, kBitrate % 10, kMCS),
            link_statistics.LookupString(kTransmitBitrateProperty, ""));

  // New station info with VHT rate parameters.
  NewStationMessage new_vht_station;
  new_vht_station.attributes()->CreateRawAttribute(NL80211_ATTR_MAC, "BSSID");

  new_vht_station.attributes()->SetRawAttributeValue(
      NL80211_ATTR_MAC, ByteString::CreateFromHexString(endpoint->bssid_hex()));
  new_vht_station.attributes()->CreateNestedAttribute(
      NL80211_ATTR_STA_INFO, "Station Info");
  new_vht_station.attributes()->GetNestedAttributeList(
      NL80211_ATTR_STA_INFO, &station_info);
  station_info->CreateU8Attribute(NL80211_STA_INFO_SIGNAL, "Signal");
  station_info->SetU8AttributeValue(NL80211_STA_INFO_SIGNAL, kSignalValue);
  station_info->CreateNestedAttribute(NL80211_STA_INFO_TX_BITRATE,
                                      "Bitrate Info");

  // Embed transmit VHT bitrate info within the station info element.
  station_info->GetNestedAttributeList(
      NL80211_STA_INFO_TX_BITRATE, &bitrate_info);
  bitrate_info->CreateU32Attribute(NL80211_RATE_INFO_BITRATE32, "Bitrate32");
  const int32_t kVhtBitrate = 70000;
  bitrate_info->SetU32AttributeValue(NL80211_RATE_INFO_BITRATE32, kVhtBitrate);
  bitrate_info->CreateU8Attribute(NL80211_RATE_INFO_VHT_MCS, "VHT-MCS");
  const int8_t kVhtMCS = 7;
  bitrate_info->SetU8AttributeValue(NL80211_RATE_INFO_VHT_MCS, kVhtMCS);
  bitrate_info->CreateU8Attribute(NL80211_RATE_INFO_VHT_NSS, "VHT-NSS");
  const int8_t kVhtNSS = 1;
  bitrate_info->SetU8AttributeValue(NL80211_RATE_INFO_VHT_NSS, kVhtNSS);
  bitrate_info->CreateFlagAttribute(NL80211_RATE_INFO_80_MHZ_WIDTH, "VHT80");
  bitrate_info->SetFlagAttributeValue(NL80211_RATE_INFO_80_MHZ_WIDTH, true);
  bitrate_info->CreateFlagAttribute(NL80211_RATE_INFO_SHORT_GI, "SGI");
  bitrate_info->SetFlagAttributeValue(NL80211_RATE_INFO_SHORT_GI, false);
  station_info->SetNestedAttributeHasAValue(NL80211_STA_INFO_TX_BITRATE);

  new_vht_station.attributes()->SetNestedAttributeHasAValue(
      NL80211_ATTR_STA_INFO);

  EXPECT_CALL(*metrics(), NotifyWifiTxBitrate(kVhtBitrate/10));

  ReportReceivedStationInfo(new_vht_station);

  link_statistics = GetLinkStatistics();
  EXPECT_EQ(StringPrintf("%d.%d MBit/s VHT-MCS %d 80MHz VHT-NSS %d",
                         kVhtBitrate / 10, kVhtBitrate % 10, kVhtMCS, kVhtNSS),
            link_statistics.LookupString(kTransmitBitrateProperty, ""));
}

TEST_F(WiFiMainTest, EAPCertification) {
  MockWiFiServiceRefPtr service = MakeMockService(kSecurity8021x);
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

  const uint32_t kDepth = 123;
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
  StartWiFi();
  ScopedMockLog log;
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _, EndsWith("no current service.")));
  EXPECT_CALL(*eap_state_handler_, ParseStatus(_, _, _)).Times(0);
  const string kEAPStatus("eap-status");
  const string kEAPParameter("eap-parameter");
  ReportEAPEvent(kEAPStatus, kEAPParameter);
  Mock::VerifyAndClearExpectations(&log);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());

  MockWiFiServiceRefPtr service = MakeMockService(kSecurity8021x);
  EXPECT_CALL(*service, SetFailure(_)).Times(0);
  EXPECT_CALL(*eap_state_handler_, ParseStatus(kEAPStatus, kEAPParameter, _));
  SetCurrentService(service);
  ReportEAPEvent(kEAPStatus, kEAPParameter);
  Mock::VerifyAndClearExpectations(service);
  Mock::VerifyAndClearExpectations(eap_state_handler_);

  EXPECT_CALL(*eap_state_handler_, ParseStatus(kEAPStatus, kEAPParameter, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(Service::kFailureOutOfRange),
                Return(false)));
  EXPECT_CALL(*service, DisconnectWithFailure(Service::kFailureOutOfRange,
                                              _,
                                              StrEq("EAPEventTask")));
  ReportEAPEvent(kEAPStatus, kEAPParameter);

  MockEapCredentials *eap = new MockEapCredentials();
  service->eap_.reset(eap);  // Passes ownership.
  const char kNetworkRpcId[] = "/service/network/rpcid";
  SetServiceNetworkRpcId(service, kNetworkRpcId);
  EXPECT_CALL(*eap_state_handler_, ParseStatus(kEAPStatus, kEAPParameter, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(Service::kFailurePinMissing),
                Return(false)));
  // We need a real string object since it will be returned by reference below.
  const string kEmptyPin;
  EXPECT_CALL(*eap, pin()).WillOnce(ReturnRef(kEmptyPin));
  EXPECT_CALL(*service, DisconnectWithFailure(Service::kFailurePinMissing,
                                              _,
                                              StrEq("EAPEventTask")));
  ReportEAPEvent(kEAPStatus, kEAPParameter);

  EXPECT_CALL(*eap_state_handler_, ParseStatus(kEAPStatus, kEAPParameter, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(Service::kFailurePinMissing),
                Return(false)));
  // We need a real string object since it will be returned by reference below.
  const string kPin("000000");
  EXPECT_CALL(*eap, pin()).WillOnce(ReturnRef(kPin));
  EXPECT_CALL(*service, DisconnectWithFailure(_, _, _)).Times(0);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(),
              NetworkReply(StrEq(kNetworkRpcId),
                           StrEq(WPASupplicant::kEAPRequestedParameterPIN),
                           Ref(kPin)));
  ReportEAPEvent(kEAPStatus, kEAPParameter);
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

struct BSS {
  string bsspath;
  string ssid;
  string bssid;
  int16_t signal_strength;
  uint16_t frequency;
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
  }
}

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

// Custom property setters should return false, and make no changes, if
// the new value is the same as the old value.
TEST_F(WiFiMainTest, CustomSetterNoopChange) {
  // SetBgscanShortInterval
  {
    Error error;
    static const uint16_t kKnownScanInterval = 4;
    // Set to known value.
    EXPECT_TRUE(SetBgscanShortInterval(kKnownScanInterval, &error));
    EXPECT_TRUE(error.IsSuccess());
    // Set to same value.
    EXPECT_FALSE(SetBgscanShortInterval(kKnownScanInterval, &error));
    EXPECT_TRUE(error.IsSuccess());
  }

  // SetBgscanSignalThreshold
  {
    Error error;
    static const int32_t kKnownSignalThreshold = 4;
    // Set to known value.
    EXPECT_TRUE(SetBgscanSignalThreshold(kKnownSignalThreshold, &error));
    EXPECT_TRUE(error.IsSuccess());
    // Set to same value.
    EXPECT_FALSE(SetBgscanSignalThreshold(kKnownSignalThreshold, &error));
    EXPECT_TRUE(error.IsSuccess());
  }

  // SetScanInterval
  {
    Error error;
    EXPECT_FALSE(SetScanInterval(GetScanInterval(), &error));
    EXPECT_TRUE(error.IsSuccess());
  }
}

// The following tests check the scan_state_ / scan_method_ state machine.

TEST_F(WiFiMainTest, FullScanFindsNothing) {
  StartScan(WiFi::kScanMethodFull);
  ReportScanDone();
  ExpectScanStop();
  ExpectFoundNothing();
  NiceScopedMockLog log;
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(10);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("FULL_NOCONNECTION ->")));
  EXPECT_CALL(*manager(), OnDeviceGeolocationInfoUpdated(_));
  dispatcher_.DispatchPendingEvents();  // Launch UpdateScanStateAfterScanDone
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);

  ScopeLogger::GetInstance()->set_verbose_level(0);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
}

TEST_F(WiFiMainTest, FullScanConnectingToConnected) {
  StartScan(WiFi::kScanMethodFull);
  WiFiEndpointRefPtr endpoint;
  ::DBus::Path bss_path;
  MockWiFiServiceRefPtr service = AttemptConnection(WiFi::kScanMethodFull,
                                                    &endpoint,
                                                    &bss_path);

  // Complete the connection.
  ExpectConnected();
  EXPECT_CALL(*service, NotifyCurrentEndpoint(EndpointMatch(endpoint)));
  NiceScopedMockLog log;
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(10);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("-> FULL_CONNECTED")));
  ReportCurrentBSSChanged(bss_path);
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);

  ScopeLogger::GetInstance()->set_verbose_level(0);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
}

TEST_F(WiFiMainTest, ProgressiveScanConnectingToConnected) {
  StartScan(WiFi::kScanMethodProgressive);
  WiFiEndpointRefPtr endpoint;
  ::DBus::Path bss_path;
  MockWiFiServiceRefPtr service = AttemptConnection(
      WiFi::kScanMethodProgressive, &endpoint, &bss_path);

  // Complete the connection.
  ExpectConnected();
  EXPECT_CALL(*service, NotifyCurrentEndpoint(EndpointMatch(endpoint)));
  NiceScopedMockLog log;
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(10);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("-> PROGRESSIVE_CONNECTED")));
  ReportCurrentBSSChanged(bss_path);
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);

  ScopeLogger::GetInstance()->set_verbose_level(0);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
}

TEST_F(WiFiMainTest, ProgressiveScanConnectingToNotFound) {
  StartScan(WiFi::kScanMethodProgressive);
  WiFiEndpointRefPtr endpoint;
  MockWiFiServiceRefPtr service = AttemptConnection(
      WiFi::kScanMethodProgressive, &endpoint, nullptr);

  // Simulate connection timeout.
  ExpectFoundNothing();
  EXPECT_CALL(*service,
              NotifyCurrentEndpoint(EndpointMatch(endpoint))).Times(0);
  NiceScopedMockLog log;
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(10);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log,
              Log(_, _, HasSubstr("-> PROGRESSIVE_FINISHED_NOCONNECTION")));
  EXPECT_CALL(*metrics(), NotifyDeviceConnectFinished(_)).Times(0);
  TimeoutPendingConnection();
  ScopeLogger::GetInstance()->set_verbose_level(0);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);
}

TEST_F(WiFiMainTest, ScanStateUma) {
  EXPECT_CALL(*metrics(), SendEnumToUMA(Metrics::kMetricScanResult, _, _)).
      Times(0);
  EXPECT_CALL(*metrics(), NotifyDeviceScanStarted(_));
  SetScanState(WiFi::kScanScanning, WiFi::kScanMethodProgressive, __func__);

  EXPECT_CALL(*metrics(), NotifyDeviceScanFinished(_));
  EXPECT_CALL(*metrics(), NotifyDeviceConnectStarted(_, _));
  SetScanState(WiFi::kScanConnecting, WiFi::kScanMethodProgressive, __func__);

  ExpectScanIdle();  // After connected.
  EXPECT_CALL(*metrics(), NotifyDeviceConnectFinished(_));
  EXPECT_CALL(*metrics(), SendEnumToUMA(Metrics::kMetricScanResult, _, _));
  SetScanState(WiFi::kScanConnected, WiFi::kScanMethodProgressive, __func__);
}

TEST_F(WiFiMainTest, ScanStateNotScanningNoUma) {
  EXPECT_CALL(*metrics(), NotifyDeviceScanStarted(_)).Times(0);
  EXPECT_CALL(*metrics(), NotifyDeviceConnectStarted(_, _));
  SetScanState(WiFi::kScanConnecting, WiFi::kScanMethodNone, __func__);

  ExpectScanIdle();  // After connected.
  EXPECT_CALL(*metrics(), NotifyDeviceConnectFinished(_));
  EXPECT_CALL(*metrics(), SendEnumToUMA(Metrics::kMetricScanResult, _, _)).
      Times(0);
  SetScanState(WiFi::kScanConnected, WiFi::kScanMethodNone, __func__);
}

TEST_F(WiFiMainTest, ConnectToServiceNotPending) {
  // Test for SetPendingService(NULL), condition a)
  // |ConnectTo|->|DisconnectFrom|.
  StartScan(WiFi::kScanMethodProgressive);

  // Setup pending service.
  ExpectScanStop();
  ExpectConnecting();
  MockWiFiServiceRefPtr service_pending(
      SetupConnectingService(DBus::Path(), NULL, NULL));
  EXPECT_EQ(service_pending.get(), GetPendingService().get());

  // ConnectTo a different service than the pending one.
  ExpectConnecting();
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Disconnect());
  NiceScopedMockLog log;
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(10);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("-> TRANSITION_TO_CONNECTING")));
  EXPECT_CALL(log, Log(_, _, HasSubstr("-> PROGRESSIVE_CONNECTING")));
  MockWiFiServiceRefPtr service_connecting(
      SetupConnectingService(DBus::Path(), NULL, NULL));
  ScopeLogger::GetInstance()->set_verbose_level(0);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
  EXPECT_EQ(service_connecting.get(), GetPendingService().get());
  EXPECT_EQ(NULL, GetCurrentService().get());
  VerifyScanState(WiFi::kScanConnecting, WiFi::kScanMethodProgressive);

  ExpectScanIdle();  // To silence messages from the destructor.
}

TEST_F(WiFiMainTest, ConnectToWithError) {
  StartScan(WiFi::kScanMethodProgressive);

  ExpectScanIdle();
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), AddNetwork(_)).
      WillOnce(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.InterfaceUnknown",
              "test threw fi.w1.wpa_supplicant1.InterfaceUnknown")));
  EXPECT_CALL(*metrics(), NotifyDeviceScanFinished(_)).Times(0);
  EXPECT_CALL(*metrics(), SendEnumToUMA(Metrics::kMetricScanResult, _, _)).
      Times(0);
  EXPECT_CALL(*adaptor_, EmitBoolChanged(kScanningProperty, false));
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
  EXPECT_CALL(*service, GetSupplicantConfigurationParameters());
  InitiateConnect(service);
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);
  EXPECT_TRUE(IsScanSessionNull());
}

TEST_F(WiFiMainTest, ScanStateHandleDisconnect) {
  // Test for SetPendingService(NULL), condition d) Disconnect while scanning.
  // Start scanning.
  StartScan(WiFi::kScanMethodProgressive);

  // Set the pending service.
  ReportScanDoneKeepScanSession();
  ExpectScanStop();
  ExpectConnecting();
  MockWiFiServiceRefPtr service = MakeMockService(kSecurityNone);
  SetPendingService(service);
  VerifyScanState(WiFi::kScanConnecting, WiFi::kScanMethodProgressive);

  // Disconnect from the pending service.
  ExpectScanIdle();
  EXPECT_CALL(*metrics(), NotifyDeviceScanFinished(_)).Times(0);
  EXPECT_CALL(*metrics(), SendEnumToUMA(Metrics::kMetricScanResult, _, _)).
      Times(0);
  ReportCurrentBSSChanged(WPASupplicant::kCurrentBSSNull);
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);
}

TEST_F(WiFiMainTest, ConnectWhileNotScanning) {
  // Setup WiFi but terminate scan.
  EXPECT_CALL(*adaptor_, EmitBoolChanged(kPoweredProperty, _)).
      Times(AnyNumber());

  ExpectScanStart(WiFi::kScanMethodProgressive, false);
  StartWiFi();
  dispatcher_.DispatchPendingEvents();

  ExpectScanStop();
  ExpectFoundNothing();
  ReportScanDone();
  dispatcher_.DispatchPendingEvents();
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);

  // Connecting.
  ExpectConnecting();
  EXPECT_CALL(*metrics(), NotifyDeviceScanStarted(_)).Times(0);
  WiFiEndpointRefPtr endpoint;
  ::DBus::Path bss_path;
  NiceScopedMockLog log;
  ScopeLogger::GetInstance()->EnableScopesByName("wifi");
  ScopeLogger::GetInstance()->set_verbose_level(10);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, HasSubstr("-> TRANSITION_TO_CONNECTING"))).
      Times(0);
  EXPECT_CALL(log, Log(_, _, HasSubstr("-> CONNECTING (not scan related)")));
  MockWiFiServiceRefPtr service =
      SetupConnectingService(DBus::Path(), &endpoint, &bss_path);

  // Connected.
  ExpectConnected();
  EXPECT_CALL(log, Log(_, _, HasSubstr("-> CONNECTED (not scan related")));
  ReportCurrentBSSChanged(bss_path);
  ScopeLogger::GetInstance()->set_verbose_level(0);
  ScopeLogger::GetInstance()->EnableScopesByName("-wifi");
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);
}

TEST_F(WiFiMainTest, BackgroundScan) {
  StartWiFi();
  SetupConnectedService(DBus::Path(), NULL, NULL);
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);

  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(1);
  TriggerScan(WiFi::kScanMethodFull);
  dispatcher_.DispatchPendingEvents();
  VerifyScanState(WiFi::kScanBackgroundScanning, WiFi::kScanMethodFull);

  ReportScanDone();
  EXPECT_CALL(*manager(), OnDeviceGeolocationInfoUpdated(_));
  dispatcher_.DispatchPendingEvents();  // Launch UpdateScanStateAfterScanDone
  VerifyScanState(WiFi::kScanIdle, WiFi::kScanMethodNone);
}

TEST_F(WiFiMainTest, ProgressiveScanDuringFull) {
  StartScan(WiFi::kScanMethodFull);

  // Now, try to slam-in a progressive scan.
  EXPECT_CALL(*scan_session_, InitiateScan()).Times(0);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  TriggerScan(WiFi::kScanMethodProgressive);
  dispatcher_.DispatchPendingEvents();
  VerifyScanState(WiFi::kScanScanning, WiFi::kScanMethodFull);

  // And, for the destructor.
  ExpectScanStop();
  ExpectScanIdle();
}

TEST_F(WiFiMainTest, FullScanDuringProgressive) {
  StartScan(WiFi::kScanMethodProgressive);

  // Now, try to slam-in a full scan.
  EXPECT_CALL(*scan_session_, InitiateScan()).Times(0);
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), Scan(_)).Times(0);
  TriggerScan(WiFi::kScanMethodFull);
  dispatcher_.DispatchPendingEvents();
  VerifyScanState(WiFi::kScanScanning, WiFi::kScanMethodProgressive);

  // And, for the destructor.
  ExpectScanStop();
  ExpectScanIdle();
}

TEST_F(WiFiMainTest, TDLSInterfaceFunctions) {
  StartWiFi();
  const char kPeer[] = "peer";

  EXPECT_CALL(*GetSupplicantInterfaceProxy(), TDLSDiscover(StrEq(kPeer)))
      .WillOnce(Return())
      .WillOnce(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.UnknownError",
              "test threw fi.w1.wpa_supplicant1.UnknownError")));
  EXPECT_TRUE(TDLSDiscover(kPeer));
  EXPECT_FALSE(TDLSDiscover(kPeer));
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());

  EXPECT_CALL(*GetSupplicantInterfaceProxy(), TDLSSetup(StrEq(kPeer)))
      .WillOnce(Return())
      .WillOnce(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.UnknownError",
              "test threw fi.w1.wpa_supplicant1.UnknownError")));
  EXPECT_TRUE(TDLSSetup(kPeer));
  EXPECT_FALSE(TDLSSetup(kPeer));
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());

  const char kStatus[] = "peachy keen";
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), TDLSStatus(StrEq(kPeer)))
      .WillOnce(Return(kStatus))
      .WillOnce(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.UnknownError",
              "test threw fi.w1.wpa_supplicant1.UnknownError")));
  EXPECT_EQ(kStatus, TDLSStatus(kPeer));
  EXPECT_EQ("", TDLSStatus(kPeer));
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());

  EXPECT_CALL(*GetSupplicantInterfaceProxy(), TDLSTeardown(StrEq(kPeer)))
      .WillOnce(Return())
      .WillOnce(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.UnknownError",
              "test threw fi.w1.wpa_supplicant1.UnknownError")));
  EXPECT_TRUE(TDLSTeardown(kPeer));
  EXPECT_FALSE(TDLSTeardown(kPeer));
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
}

TEST_F(WiFiMainTest, PerformTDLSOperation) {
  StartWiFi();
  const char kPeer[] = "00:11:22:33:44:55";

  {
    Error error;
    EXPECT_EQ("", PerformTDLSOperation("Do the thing", kPeer, &error));
    EXPECT_EQ(Error::kInvalidArguments, error.type());
  }

  {
    Error error;
    EXPECT_EQ("", PerformTDLSOperation(kTDLSDiscoverOperation, "peer", &error));
    // This is not a valid IP address nor is it a MAC address.
    EXPECT_EQ(Error::kInvalidArguments, error.type());
  }

  const char kAddress[] = "192.168.1.1";
  EXPECT_CALL(*manager(), device_info()).WillRepeatedly(Return(device_info()));

  {
    // The provided IP address is not local.
    EXPECT_CALL(*device_info(), HasDirectConnectivityTo(kInterfaceIndex, _))
        .WillOnce(Return(false));
    Error error;
    EXPECT_EQ("", PerformTDLSOperation(kTDLSDiscoverOperation,
                                       kAddress, &error));
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    Mock::VerifyAndClearExpectations(device_info());
  }

  {
    // If the MAC address of the peer is in the ARP cache, we should
    // perform the TDLS operation on the resolved MAC.
    const char kResolvedMac[] = "00:11:22:33:44:55";
    const ByteString kMacBytes(
        WiFiEndpoint::MakeHardwareAddressFromString(kResolvedMac));
    EXPECT_CALL(*device_info(), HasDirectConnectivityTo(kInterfaceIndex, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*device_info(), GetMACAddressOfPeer(kInterfaceIndex, _, _))
        .WillOnce(DoAll(SetArgumentPointee<2>(kMacBytes), Return(true)));
    EXPECT_CALL(*GetSupplicantInterfaceProxy(),
                TDLSDiscover(StrEq(kResolvedMac)));
    Error error;
    EXPECT_EQ("", PerformTDLSOperation(kTDLSDiscoverOperation,
                                       kAddress, &error));
    EXPECT_TRUE(error.IsSuccess());
    Mock::VerifyAndClearExpectations(device_info());
    Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  }

  // This is the same test as TDLSInterfaceFunctions above, but using the
  // method called by the RPC adapter.
  EXPECT_CALL(*GetSupplicantInterfaceProxy(), TDLSDiscover(StrEq(kPeer)))
      .WillOnce(Return())
      .WillOnce(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.UnknownError",
              "test threw fi.w1.wpa_supplicant1.UnknownError")));
  {
    Error error;
    EXPECT_EQ("", PerformTDLSOperation(kTDLSDiscoverOperation, kPeer, &error));
    EXPECT_TRUE(error.IsSuccess());
  }
  {
    Error error;
    EXPECT_EQ("", PerformTDLSOperation(kTDLSDiscoverOperation, kPeer, &error));
    EXPECT_EQ(Error::kOperationFailed, error.type());
  }
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());

  EXPECT_CALL(*GetSupplicantInterfaceProxy(), TDLSSetup(StrEq(kPeer)))
      .WillOnce(Return())
      .WillOnce(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.UnknownError",
              "test threw fi.w1.wpa_supplicant1.UnknownError")));
  {
    Error error;
    EXPECT_EQ("", PerformTDLSOperation(kTDLSSetupOperation, kPeer, &error));
    EXPECT_TRUE(error.IsSuccess());
  }
  {
    Error error;
    EXPECT_EQ("", PerformTDLSOperation(kTDLSSetupOperation, kPeer, &error));
    EXPECT_EQ(Error::kOperationFailed, error.type());
  }
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());


  const map<string, string> kTDLSStatusMap {
    { "Baby, I don't care", kTDLSUnknownState },
    { WPASupplicant::kTDLSStateConnected, kTDLSConnectedState },
    { WPASupplicant::kTDLSStateDisabled, kTDLSDisabledState },
    { WPASupplicant::kTDLSStatePeerDoesNotExist, kTDLSNonexistentState },
    { WPASupplicant::kTDLSStatePeerNotConnected, kTDLSDisconnectedState },
  };

  for (const auto &it : kTDLSStatusMap) {
    EXPECT_CALL(*GetSupplicantInterfaceProxy(), TDLSStatus(StrEq(kPeer)))
        .WillOnce(Return(it.first));
    Error error;
    EXPECT_EQ(it.second,
              PerformTDLSOperation(kTDLSStatusOperation, kPeer, &error));
    EXPECT_TRUE(error.IsSuccess());
    Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());
  }

  EXPECT_CALL(*GetSupplicantInterfaceProxy(), TDLSStatus(StrEq(kPeer)))
      .WillOnce(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.UnknownError",
              "test threw fi.w1.wpa_supplicant1.UnknownError")));
  {
    Error error;
    EXPECT_EQ("", PerformTDLSOperation(kTDLSStatusOperation, kPeer, &error));
    EXPECT_EQ(Error::kOperationFailed, error.type());
  }
  Mock::VerifyAndClearExpectations(GetSupplicantInterfaceProxy());

  EXPECT_CALL(*GetSupplicantInterfaceProxy(), TDLSTeardown(StrEq(kPeer)))
      .WillOnce(Return())
      .WillOnce(Throw(
          DBus::Error(
              "fi.w1.wpa_supplicant1.UnknownError",
              "test threw fi.w1.wpa_supplicant1.UnknownError")));
  {
    Error error;
    EXPECT_EQ("", PerformTDLSOperation(kTDLSTeardownOperation, kPeer, &error));
    EXPECT_TRUE(error.IsSuccess());
  }
  {
    Error error;
    EXPECT_EQ("", PerformTDLSOperation(kTDLSTeardownOperation, kPeer, &error));
    EXPECT_EQ(Error::kOperationFailed, error.type());
  }
}

TEST_F(WiFiMainTest, OnNewWiphy) {
  Nl80211Message new_wiphy_message(
      NewWiphyMessage::kCommand, NewWiphyMessage::kCommandString);
  new_wiphy_message.attributes()->
      CreateStringAttribute(Nl80211AttributeWiphyName::kName,
                            Nl80211AttributeWiphyName::kNameString);
  new_wiphy_message.attributes()->
      SetStringAttributeValue(Nl80211AttributeWiphyName::kName,
                              "test-phy");
  EXPECT_CALL(*mac80211_monitor(), Start(_));
  OnNewWiphy(new_wiphy_message);
  // TODO(quiche): We should test the rest of OnNewWiphy, which parses
  // out frequency information.
}

TEST_F(WiFiMainTest, StateChangedUpdatesMac80211Monitor) {
  EXPECT_CALL(*mac80211_monitor(), UpdateConnectedState(true)).Times(2);
  ReportStateChanged(WPASupplicant::kInterfaceStateCompleted);
  ReportStateChanged(WPASupplicant::kInterfaceState4WayHandshake);

  EXPECT_CALL(*mac80211_monitor(), UpdateConnectedState(false));
  ReportStateChanged(WPASupplicant::kInterfaceStateAssociating);
}

TEST_F(WiFiMainTest, AddRemoveWakeOnPacketConnection) {
  const string ip_string1("192.168.0.19");
  const string ip_string2("192.168.0.55");
  const string ip_string3("192.168.0.74");
  IPAddress ip_addr1(ip_string1);
  IPAddress ip_addr2(ip_string2);
  IPAddress ip_addr3(ip_string3);
  Error e;

  GetWakeOnPacketConnections()->Clear();
  EXPECT_TRUE(GetWakeOnPacketConnections()->Empty());
  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  AddWakeOnPacketConnection(ip_addr1, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 1);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  AddWakeOnPacketConnection(ip_addr2, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 2);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  AddWakeOnPacketConnection(ip_addr3, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 3);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  RemoveWakeOnPacketConnection(ip_addr2, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 2);
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  RemoveWakeOnPacketConnection(ip_addr2, &e);
  EXPECT_EQ(e.type(), Error::kNotFound);
  EXPECT_STREQ(e.message().c_str(),
               "No such wake-on-packet connection registered");
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 2);

  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  RemoveWakeOnPacketConnection(ip_addr1, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 1);
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  AddWakeOnPacketConnection(ip_addr2, &e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 2);
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_TRUE(GetWakeOnPacketConnections()->Contains(ip_addr3));

  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  RemoveAllWakeOnPacketConnections(&e);
  EXPECT_EQ(GetWakeOnPacketConnections()->Count(), 0);
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr1));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr2));
  EXPECT_FALSE(GetWakeOnPacketConnections()->Contains(ip_addr3));
}

TEST_F(WiFiMainTest, RequestWakeOnPacketSettings) {
  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          GetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  RequestWakeOnPacketSettings();
}

TEST_F(WiFiMainTest, VerifyWakeOnPacketConnections) {
  ScopedMockLog log;
  // Create an Nl80211 response to a NL80211_CMD_GET_WOWLAN request
  // indicating that there are no wake-on-packet rules programmed into the NIC.
  const uint8_t kResponseNoIPAddresses[] = {
      0x14, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x00, 0x00, 0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00};
  scoped_ptr<uint8_t[]> message_memory(
      new uint8_t[sizeof(kResponseNoIPAddresses)]);
  memcpy(message_memory.get(), kResponseNoIPAddresses,
         sizeof(kResponseNoIPAddresses));
  nlmsghdr *hdr = reinterpret_cast<nlmsghdr *>(message_memory.get());
  GetWakeOnPacketConnMessage msg;
  msg.InitFromNlmsg(hdr);
  // Successful verification.
  EXPECT_TRUE(GetWakeOnPacketConnections()->Empty());
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, EndsWith("successfully verified")));
  VerifyWakeOnPacketConnections(msg);
  // Unsuccessful verification.
  GetWakeOnPacketConnections()->AddUnique(IPAddress("1.1.1.1"));
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _, EndsWith("structure detected")));
  VerifyWakeOnPacketConnections(msg);
}

TEST_F(WiFiMainTest, RetrySetWakeOnPacketConnections) {
  ScopedMockLog log;
  *GetNumSetWakeOnPacketRetries() = WiFi::kMaxSetWakeOnPacketRetries - 1;
  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  RetrySetWakeOnPacketConnections();
  EXPECT_EQ(*GetNumSetWakeOnPacketRetries(),
            WiFi::kMaxSetWakeOnPacketRetries);

  EXPECT_CALL(netlink_manager_, SendNl80211Message(_, _, _, _)).Times(0);
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(_, _, EndsWith("max retry attempts reached")));
  RetrySetWakeOnPacketConnections();
  EXPECT_EQ(*GetNumSetWakeOnPacketRetries(), 0);
}

TEST_F(WiFiMainTest, SendSetWakeOnPacketMessage) {
  Error e;
  EXPECT_TRUE(GetWakeOnPacketConnections()->Empty());
  EXPECT_CALL(netlink_manager_,
              SendNl80211Message(IsDisableWakeOnPacketMsg(), _, _, _)).Times(1);
  SendSetWakeOnPacketMessage(&e);

  GetWakeOnPacketConnections()->AddUnique(IPAddress("1.1.1.1"));
  EXPECT_CALL(netlink_manager_,
              SendNl80211Message(IsDisableWakeOnPacketMsg(), _, _, _)).Times(0);
  EXPECT_CALL(
      netlink_manager_,
      SendNl80211Message(IsNl80211Command(kNl80211FamilyId,
                                          SetWakeOnPacketConnMessage::kCommand),
                         _, _, _)).Times(1);
  SendSetWakeOnPacketMessage(&e);
}

}  // namespace shill
