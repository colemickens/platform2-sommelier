// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SUPPLICANT_INTERFACE_PROXY_H_
#define SUPPLICANT_INTERFACE_PROXY_H_

#include <map>
#include <string>

#include <base/basictypes.h>

#include "shill/dbus_bindings/supplicant-interface.h"
#include "shill/supplicant_interface_proxy_interface.h"
#include "shill/refptr_types.h"

namespace shill {

// SupplicantInterfaceProxy. provides access to wpa_supplicant's
// network-interface APIs via D-Bus.
class SupplicantInterfaceProxy
    : public SupplicantInterfaceProxyInterface {
 public:
  SupplicantInterfaceProxy(const WiFiRefPtr &wifi,
                           DBus::Connection *bus,
                           const ::DBus::Path &object_path,
                           const char *dbus_addr);
  virtual ~SupplicantInterfaceProxy();

  virtual ::DBus::Path AddNetwork(
      const std::map<std::string, ::DBus::Variant> &args);
  virtual void ClearCachedCredentials();
  virtual void Disconnect();
  virtual void FlushBSS(const uint32_t &age);
  virtual void RemoveAllNetworks();
  virtual void RemoveNetwork(const ::DBus::Path &network);
  virtual void Scan(
      const std::map<std::string, ::DBus::Variant> &args);
  virtual void SelectNetwork(const ::DBus::Path &network);
  virtual void SetFastReauth(bool enabled);
  virtual void SetScanInterval(int seconds);

 private:
  class Proxy : public fi::w1::wpa_supplicant1::Interface_proxy,
    public ::DBus::ObjectProxy {
   public:
    Proxy(const WiFiRefPtr &wifi,
          DBus::Connection *bus,
          const ::DBus::Path &object_path,
          const char *dbus_addr);
    virtual ~Proxy();

   private:
    // signal handlers called by dbus-c++, via
    // fi::w1::wpa_supplicant1::Interface_proxy interface
    virtual void BlobAdded(const std::string &blobname);
    virtual void BlobRemoved(const std::string &blobname);
    virtual void BSSAdded(const ::DBus::Path &BSS,
                          const std::map<std::string, ::DBus::Variant>
                          &properties);
    virtual void BSSRemoved(const ::DBus::Path &BSS);
    virtual void Certification(const std::map<std::string, ::DBus::Variant>
                               &properties);
    virtual void NetworkAdded(const ::DBus::Path &network,
                              const std::map<std::string, ::DBus::Variant>
                              &properties);
    virtual void NetworkRemoved(const ::DBus::Path &network);
    virtual void NetworkSelected(const ::DBus::Path &network);
    virtual void PropertiesChanged(const std::map<std::string, ::DBus::Variant>
                                   &properties);
    virtual void ScanDone(const bool &success);

    WiFiRefPtr wifi_;
    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(SupplicantInterfaceProxy);
};

}  // namespace shill

#endif  // SUPPLICANT_INTERFACE_PROXY_H_
