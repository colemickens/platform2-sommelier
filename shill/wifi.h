// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_
#define SHILL_WIFI_

#include <map>
#include <string>
#include <vector>

#include "shill/device.h"
#include "shill/shill_event.h"
#include "shill/supplicant-process.h"
#include "shill/supplicant-interface.h"

namespace shill {

class WiFiEndpoint;
typedef scoped_refptr<const WiFiEndpoint> WiFiEndpointConstRefPtr;
typedef scoped_refptr<WiFiEndpoint> WiFiEndpointRefPtr;

// WiFi class. Specialization of Device for WiFi.
class WiFi : public Device {
 public:
  WiFi(ControlInterface *control_interface,
       EventDispatcher *dispatcher,
       Manager *manager,
       const std::string& link,
       int interface_index);
  virtual ~WiFi();
  virtual void Start();
  virtual void Stop();
  virtual bool TechnologyIs(const Technology type);

  // called by SupplicantInterfaceProxy, in response to events from
  // wpa_supplicant.
  void BSSAdded(const ::DBus::Path &BSS,
                const std::map<std::string, ::DBus::Variant> &properties);
  void ScanDone();

  // called by WiFiService, to effect changes to wpa_supplicant
  ::DBus::Path AddNetwork(
      const std::map<std::string, ::DBus::Variant> &args);
  void SelectNetwork(const ::DBus::Path &network);

 private:
  // SupplicantProcessProxy. provides access to wpa_supplicant's
  // process-level D-Bus APIs.
  class SupplicantProcessProxy :
      public fi::w1::wpa_supplicant1_proxy,
        private ::DBus::ObjectProxy  // used by dbus-c++, not WiFi
  {
   public:
    explicit SupplicantProcessProxy(DBus::Connection *bus);

   private:
    // called by dbus-c++, via wpa_supplicant1_proxy interface,
    // in response to signals from wpa_supplicant. not exposed
    // to WiFi.
    virtual void InterfaceAdded(
        const ::DBus::Path &path,
        const std::map<std::string, ::DBus::Variant> &properties);
    virtual void InterfaceRemoved(const ::DBus::Path &path);
    virtual void PropertiesChanged(
        const std::map<std::string, ::DBus::Variant> &properties);
  };

  // SupplicantInterfaceProxy. provides access to wpa_supplicant's
  // network-interface D-Bus APIs.
  class SupplicantInterfaceProxy :
      public fi::w1::wpa_supplicant1::Interface_proxy,
      private ::DBus::ObjectProxy  // used by dbus-c++, not WiFi
  {
   public:
    SupplicantInterfaceProxy(WiFi *wifi, DBus::Connection *bus,
                             const ::DBus::Path &object_path);

   private:
    // called by dbus-c++, via Interface_proxy interface,
    // in response to signals from wpa_supplicant. not exposed
    // to WiFi.
    virtual void ScanDone(const bool &success);
    virtual void BSSAdded(const ::DBus::Path &BSS,
                          const std::map<std::string, ::DBus::Variant>
                          &properties);
    virtual void BSSRemoved(const ::DBus::Path &BSS);
    virtual void BlobAdded(const std::string &blobname);
    virtual void BlobRemoved(const std::string &blobname);
    virtual void NetworkAdded(const ::DBus::Path &network,
                              const std::map<std::string, ::DBus::Variant>
                              &properties);
    virtual void NetworkRemoved(const ::DBus::Path &network);
    virtual void NetworkSelected(const ::DBus::Path &network);
    virtual void PropertiesChanged(const std::map<std::string, ::DBus::Variant>
                                   &properties);

    WiFi &wifi_;
  };

  typedef std::map<const std::string, WiFiEndpointRefPtr> EndpointMap;
  typedef std::map<const std::string, ServiceRefPtr> ServiceMap;

  static const char kSupplicantPath[];
  static const char kSupplicantDBusAddr[];
  static const char kSupplicantWiFiDriver[];
  static const char kSupplicantErrorInterfaceExists[];
  static const char kSupplicantKeyModeNone[];

  void RealScanDone();

  static unsigned int service_id_serial_;
  ScopedRunnableMethodFactory<WiFi> task_factory_;
  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  DBus::Connection dbus_;
  scoped_ptr<SupplicantProcessProxy> supplicant_process_proxy_;
  scoped_ptr<SupplicantInterfaceProxy> supplicant_interface_proxy_;
  bool scan_pending_;
  EndpointMap endpoint_by_bssid_;
  ServiceMap service_by_private_id_;

  // provide WiFiTest access to scan_pending_, so it can determine
  // if the scan completed, or timed out.
  friend class WiFiTest;
  DISALLOW_COPY_AND_ASSIGN(WiFi);
};

}  // namespace shill

#endif  // SHILL_WIFI_
