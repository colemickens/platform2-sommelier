// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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
  virtual void Scan(Error *error);
  virtual bool TechnologyIs(const Technology::Identifier type) const;
  virtual void LinkEvent(unsigned int flags, unsigned int change);

  // called by SupplicantInterfaceProxy, in response to events from
  // wpa_supplicant.
  void BSSAdded(const ::DBus::Path &BSS,
                const std::map<std::string, ::DBus::Variant> &properties);
  void PropertiesChanged(
      const std::map<std::string, ::DBus::Variant> &properties);
  void ScanDone();

  // called by WiFiService
  virtual void ConnectTo(
      WiFiService *service,
      const std::map<std::string, ::DBus::Variant> &service_params);

  // called by Manager
  virtual WiFiServiceRefPtr GetService(const KeyValueStore &args, Error *error);

 private:
  friend class WiFiMainTest;  // access to supplicant_*_proxy_, link_up_
  FRIEND_TEST(WiFiMainTest, FindServiceWEP);
  FRIEND_TEST(WiFiMainTest, FindServiceWPA);
  FRIEND_TEST(WiFiMainTest, InitialSupplicantState);  // kInterfaceStateUnknown

  typedef std::map<const std::string, WiFiEndpointRefPtr> EndpointMap;
  typedef std::map<const std::string, WiFiServiceRefPtr> ServiceMap;

  static const char kManagerErrorPassphraseRequired[];
  static const char kManagerErrorSSIDTooLong[];
  static const char kManagerErrorSSIDTooShort[];
  static const char kManagerErrorSSIDRequired[];
  static const char kManagerErrorTypeRequired[];
  static const char kManagerErrorUnsupportedSecurityMode[];
  static const char kManagerErrorUnsupportedServiceType[];
  static const char kManagerErrorUnsupportedServiceMode[];
  static const char kInterfaceStateUnknown[];

  WiFiServiceRefPtr FindService(const std::vector<uint8_t> &ssid,
                                const std::string &mode,
                                const std::string &security) const;
  ByteArrays GetHiddenSSIDList();
  void ScanDoneTask();
  void ScanTask();
  void StateChanged(const std::string &new_state);

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  ScopedRunnableMethodFactory<WiFi> task_factory_;
  scoped_ptr<SupplicantProcessProxyInterface> supplicant_process_proxy_;
  scoped_ptr<SupplicantInterfaceProxyInterface> supplicant_interface_proxy_;
  EndpointMap endpoint_by_bssid_;
  ServiceMap service_by_private_id_;

  // Properties
  std::string bgscan_method_;
  uint16 bgscan_short_interval_;
  int32 bgscan_signal_threshold_;
  bool scan_pending_;
  uint16 scan_interval_;
  bool link_up_;
  std::vector<WiFiServiceRefPtr> services_;
  WiFiServiceRefPtr pending_service_;
  std::string supplicant_state_;

  DISALLOW_COPY_AND_ASSIGN(WiFi);
};

}  // namespace shill

#endif  // SHILL_WIFI_
