// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_
#define SHILL_WIFI_

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/device.h"
#include "shill/event_dispatcher.h"
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
  virtual void LinkEvent(unsigned int flags, unsigned int change);

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

  // Called by Manager.
  virtual WiFiServiceRefPtr GetService(const KeyValueStore &args, Error *error);

 private:
  friend class WiFiMainTest;  // access to supplicant_*_proxy_, link_up_
  FRIEND_TEST(WiFiMainTest, InitialSupplicantState);  // kInterfaceStateUnknown
  FRIEND_TEST(WiFiMainTest, ScanResultsWithUpdates);  // EndpointMap

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
  static const char kManagerErrorTypeRequired[];
  static const char kManagerErrorUnsupportedSecurityMode[];
  static const char kManagerErrorUnsupportedServiceType[];
  static const char kManagerErrorUnsupportedServiceMode[];
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

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  ScopedRunnableMethodFactory<WiFi> task_factory_;
  scoped_ptr<SupplicantProcessProxyInterface> supplicant_process_proxy_;
  scoped_ptr<SupplicantInterfaceProxyInterface> supplicant_interface_proxy_;
  // The rpcid used as the key is wpa_supplicant's D-Bus path for the
  // Endpoint (BSS, in supplicant parlance).
  EndpointMap endpoint_by_rpcid_;
  // Map from Services to the D-Bus path for the corresponding wpa_supplicant
  // Network.
  ReverseServiceMap rpcid_by_service_;
  bool link_up_;
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
