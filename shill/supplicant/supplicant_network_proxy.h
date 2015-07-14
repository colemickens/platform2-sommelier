// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SUPPLICANT_SUPPLICANT_NETWORK_PROXY_H_
#define SHILL_SUPPLICANT_SUPPLICANT_NETWORK_PROXY_H_

#include <map>
#include <string>

#include <base/macros.h>

#include "shill/dbus_proxies/supplicant-network.h"
#include "shill/refptr_types.h"
#include "shill/supplicant/supplicant_network_proxy_interface.h"

namespace shill {

// SupplicantNetworkProxy. provides access to wpa_supplicant's
// network-interface APIs via D-Bus.
class SupplicantNetworkProxy
    : public SupplicantNetworkProxyInterface {
 public:
  SupplicantNetworkProxy(DBus::Connection* bus,
                         const ::DBus::Path& object_path,
                         const char* dbus_addr);
  ~SupplicantNetworkProxy() override;

  bool SetEnabled(bool enabled) override;

 private:
  class Proxy : public fi::w1::wpa_supplicant1::Network_proxy,
    public ::DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* bus,
          const ::DBus::Path& object_path,
          const char* dbus_addr);
    ~Proxy() override;

   private:
    // signal handlers called by dbus-c++, via
    // fi::w1::wpa_supplicant1::Network_proxy interface
    void PropertiesChanged(const std::map<std::string, ::DBus::Variant>
                           &properties) override;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(SupplicantNetworkProxy);
};

}  // namespace shill

#endif  // SHILL_SUPPLICANT_SUPPLICANT_NETWORK_PROXY_H_
