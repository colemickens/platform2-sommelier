// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_
#define SHILL_WIFI_

// A WiFi device represents a wireless network interface implemented as an IEEE
// 802.11 station.  An Access Point (AP) (or, more correctly, a Basic Service
// Set(BSS)) is represented by a WiFiEndpoint.  An AP provides a WiFiService,
// which is the same concept as Extended Service Set (ESS) in 802.11,
// identified by an SSID.  A WiFiService includes zero or more WiFiEndpoints
// that provide that service.
//
// A WiFi device interacts with a real device through WPA Supplicant.
// Wifi::Start() creates a connection to WPA Supplicant, represented by
// |supplicant_interface_proxy_|.  [1]
//
// A WiFi device becomes aware of WiFiEndpoints through BSSAdded signals from
// WPA Supplicant, which identifies them by a "path".  The WiFi object maintains
// an EndpointMap in |endpoint_by_rpcid_|, in which the key is the "path" and
// the value is a pointer to a WiFiEndpoint object.  When a WiFiEndpoint is
// added, it is associated with a WiFiService.
//
// A WiFi device becomes aware of a WiFiService in three different ways.  1)
// When a WiFiEndpoint is added through the BSSAdded signal, the WiFiEndpoint is
// providing a service, and if that service is unknown to the WiFi device, it is
// added at that point.  2) The Manager can add a WiFiService by calling
// WiFi::GetService().  3) Services are loaded from the profile through a call
// to WiFi::Load().
//
// The WiFi device connects to a WiFiService, not a WiFiEndpoint, through WPA
// Supplicant. It is the job of WPA Supplicant to select a BSS (aka
// WiFiEndpoint) to connect to.  The protocol for establishing a connection is
// as follows:
//
//  1.  The WiFi device sends AddNetwork to WPA Supplicant, which returns a
//  "network path" when done.
//
//  2.  The WiFi device sends SelectNetwork, indicating the network path
//  received in 1, to WPA Supplicant, which begins the process of associating
//  with an AP in the ESS.  At this point the WiFiService which is being
//  connected is called the |pending_service_|.
//
// 3.  When association is complete, WPA Supplicant sends a PropertiesChanged
// signal to the WiFi device, indicating a change in the CurrentBSS.  The
// WiFiService indicated by the new value of CurrentBSS is set as the
// |current_service_|, and |pending_service_| is (normally) cleared.
//
// Some key things to notice are 1) WPA Supplicant does the work of selecting
// the AP (aka WiFiEndpoint) and it tells the WiFi device which AP it selected.
// 2) The process of connecting is asynchronous.  There is a |current_service_|
// to which the WiFi device is presently using and a |pending_service_| to which
// the WiFi device has initiated a connection.
//
// A WiFi device is notified that an AP has gone away via the BSSRemoved signal.
// When the last WiFiEndpoint of a WiFiService is removed, the WiFiService
// itself is deleted.
//
// TODO(gmorain): Add explanation of hidden SSIDs.
//
// WPA Supplicant's PropertiesChanged signal communicates changes in the state
// of WPA Supplicant's current service.  This state is stored in
// |supplicant_state_| and reflects WPA Supplicant's view of the state of the
// connection to an AP.  Changes in this state sometimes cause state changes in
// the WiFiService to which a WiFi device is connected.  For example, when WPA
// Supplicant signals the new state to be "completed", then the WiFiService
// state gets changed to "configuring".  State change notifications are not
// reliable because WPA Supplicant may coalesce state changes in quick
// succession so that only the last of the changes is signaled.
//
// Notes:
//
// 1.  Shill's definition of the interface is described in
// shill/dbus_bindings/supplicant-interface.xml, and the WPA Supplicant's
// description of the same interface is in
// third_party/wpa_supplicant/doc/dbus.doxygen.

#include <time.h>

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/device.h"
#include "shill/event_dispatcher.h"
#include "shill/power_manager.h"
#include "shill/refptr_types.h"

namespace shill {

class Error;
class KeyValueStore;
class ProxyFactory;
class SupplicantInterfaceProxyInterface;
class SupplicantProcessProxyInterface;
class WiFiService;

// WiFi class. Specialization of Device for WiFi.
class WiFi : public Device {
 public:
  WiFi(ControlInterface *control_interface,
       EventDispatcher *dispatcher,
       Metrics *metrics,
       Manager *manager,
       const std::string &link,
       const std::string &address,
       int interface_index);
  virtual ~WiFi();

  virtual void Start();
  virtual void Stop();
  virtual bool Load(StoreInterface *storage);
  virtual void Scan(Error *error);
  virtual bool TechnologyIs(const Technology::Identifier type) const;
  virtual bool IsConnectingTo(const WiFiService &service) const;

  // Called by SupplicantInterfaceProxy, in response to events from
  // wpa_supplicant.
  void BSSAdded(const ::DBus::Path &BSS,
                const std::map<std::string, ::DBus::Variant> &properties);
  void BSSRemoved(const ::DBus::Path &BSS);
  void PropertiesChanged(
      const std::map<std::string, ::DBus::Variant> &properties);
  void ScanDone();

  // Called by WiFiService.
  virtual void ConnectTo(
      WiFiService *service,
      std::map<std::string, ::DBus::Variant> service_params);
  virtual void DisconnectFrom(WiFiService *service);
  virtual bool IsIdle() const;
  virtual void ClearCachedCredentials();

  // Called by WiFiEndpoint.
  virtual void NotifyEndpointChanged(const WiFiEndpoint &endpoint);

  // Called by Manager.
  virtual WiFiServiceRefPtr GetService(const KeyValueStore &args, Error *error);

  // Utility, used by WiFiService and WiFiEndpoint.
  // Replace non-ASCII characters with '?'. Return true if one or more
  // characters were changed.
  static bool SanitizeSSID(std::string *ssid);

 private:
  friend class WiFiMainTest;  // access to supplicant_*_proxy_, link_up_
  FRIEND_TEST(WiFiMainTest, InitialSupplicantState);  // kInterfaceStateUnknown
  FRIEND_TEST(WiFiMainTest, ScanResults);             // EndpointMap
  FRIEND_TEST(WiFiMainTest, ScanResultsWithUpdates);  // EndpointMap
  FRIEND_TEST(WiFiMainTest, FlushBSSOnResume);  // kMaxBSSResumeAgeSeconds
  FRIEND_TEST(WiFiPropertyTest, ClearDerivedProperty);  // bgscan_method_

  typedef std::map<const std::string, WiFiEndpointRefPtr> EndpointMap;
  typedef std::map<WiFiService *, std::string> ReverseServiceMap;

  static const char *kDefaultBgscanMethod;
  static const uint16 kDefaultBgscanShortIntervalSeconds;
  static const int32 kDefaultBgscanSignalThresholdDbm;
  static const uint16 kDefaultScanIntervalSeconds;
  static const char kManagerErrorPassphraseRequired[];
  static const char kManagerErrorSSIDTooLong[];
  static const char kManagerErrorSSIDTooShort[];
  static const char kManagerErrorSSIDRequired[];
  static const char kManagerErrorUnsupportedSecurityMode[];
  static const char kManagerErrorUnsupportedServiceMode[];
  static const time_t kMaxBSSResumeAgeSeconds;
  static const char kInterfaceStateUnknown[];

  std::string CreateBgscanConfigString();
  std::string GetBgscanMethod(Error */* error */) { return bgscan_method_; }
  uint16 GetBgscanShortInterval(Error */* error */) {
    return bgscan_short_interval_seconds_;
  }
  int32 GetBgscanSignalThreshold(Error */* error */) {
    return bgscan_signal_threshold_dbm_;
  }
  uint16 GetScanInterval(Error */* error */) { return scan_interval_seconds_; }
  void SetBgscanMethod(const std::string &method, Error *error);
  void SetBgscanShortInterval(const uint16 &seconds, Error *error);
  void SetBgscanSignalThreshold(const int32 &dbm, Error *error);
  void SetScanInterval(const uint16 &seconds, Error *error);

  WiFiServiceRefPtr CreateServiceForEndpoint(
      const WiFiEndpoint &endpoint, bool hidden_ssid);
  void CurrentBSSChanged(const ::DBus::Path &new_bss);
  WiFiServiceRefPtr FindService(const std::vector<uint8_t> &ssid,
                                const std::string &mode,
                                const std::string &security) const;
  WiFiServiceRefPtr FindServiceForEndpoint(const WiFiEndpoint &endpoint);
  ByteArrays GetHiddenSSIDList();
  void HandleDisconnect();
  void HandleRoam(const ::DBus::Path &new_bssid);
  // Create services for hidden networks stored in |storage|.  Returns true
  // if any were found, otherwise returns false.
  bool LoadHiddenServices(StoreInterface *storage);
  void BSSAddedTask(const ::DBus::Path &BSS,
                    const std::map<std::string, ::DBus::Variant> &properties);
  void BSSRemovedTask(const ::DBus::Path &BSS);
  void ClearCachedCredentialsTask();
  void PropertiesChangedTask(
      const std::map<std::string, ::DBus::Variant> &properties);
  void ScanDoneTask();
  void ScanTask();
  void StateChanged(const std::string &new_state);

  void HelpRegisterDerivedInt32(
      PropertyStore *store,
      const std::string &name,
      int32(WiFi::*get)(Error *error),
      void(WiFi::*set)(const int32 &value, Error *error));
  void HelpRegisterDerivedString(
      PropertyStore *store,
      const std::string &name,
      std::string(WiFi::*get)(Error *error),
      void(WiFi::*set)(const std::string &value, Error *error));
  void HelpRegisterDerivedUint16(
      PropertyStore *store,
      const std::string &name,
      uint16(WiFi::*get)(Error *error),
      void(WiFi::*set)(const uint16 &value, Error *error));

  // If this WiFi device is idle and |new_state| indicates a resume event, a
  // scan is initiated.
  void HandlePowerStateChange(PowerManager::SuspendState new_state);

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;
  Time *time_;

  scoped_ptr<SupplicantProcessProxyInterface> supplicant_process_proxy_;
  scoped_ptr<SupplicantInterfaceProxyInterface> supplicant_interface_proxy_;
  // The rpcid used as the key is wpa_supplicant's D-Bus path for the
  // Endpoint (BSS, in supplicant parlance).
  EndpointMap endpoint_by_rpcid_;
  // Map from Services to the D-Bus path for the corresponding wpa_supplicant
  // Network.
  ReverseServiceMap rpcid_by_service_;
  std::vector<WiFiServiceRefPtr> services_;
  // The Service we are presently connected to. May be NULL is we're not
  // not connected to any Service.
  WiFiServiceRefPtr current_service_;
  // The Service we're attempting to connect to. May be NULL if we're
  // not attempting to connect to a new Service. If non-NULL, should
  // be distinct from |current_service_|. (A service should not
  // simultaneously be both pending, and current.)
  WiFiServiceRefPtr pending_service_;
  std::string supplicant_state_;
  std::string supplicant_bss_;
  // Signifies that ClearCachedCredentialsTask() is pending.
  bool clear_cached_credentials_pending_;
  // Indicates that we should flush supplicant's BSS cache after the
  // next scan completes.
  bool need_bss_flush_;
  struct timeval resumed_at_;

  // Properties
  std::string bgscan_method_;
  uint16 bgscan_short_interval_seconds_;
  int32 bgscan_signal_threshold_dbm_;
  bool scan_pending_;
  uint16 scan_interval_seconds_;

  DISALLOW_COPY_AND_ASSIGN(WiFi);
};

}  // namespace shill

#endif  // SHILL_WIFI_
