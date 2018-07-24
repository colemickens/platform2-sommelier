// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SUPPLICANT_SUPPLICANT_BSS_PROXY_H_
#define SHILL_SUPPLICANT_SUPPLICANT_BSS_PROXY_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

#include "shill/dbus_proxies/supplicant-bss.h"
#include "shill/supplicant/supplicant_bss_proxy_interface.h"

namespace shill {

class WiFiEndpoint;

class SupplicantBSSProxy : public SupplicantBSSProxyInterface {
 public:
  SupplicantBSSProxy(WiFiEndpoint* wifi_endpoint,
                     DBus::Connection* bus,
                     const ::DBus::Path& object_path,
                     const char* dbus_addr);
  ~SupplicantBSSProxy() override;

 private:
  class Proxy : public fi::w1::wpa_supplicant1::BSS_proxy,
    public ::DBus::ObjectProxy {
   public:
    Proxy(WiFiEndpoint* wifi_endpoint,
          DBus::Connection* bus,
          const ::DBus::Path& object_path,
          const char* dbus_addr);
    ~Proxy() override;

   private:
    // signal handlers called by dbus-c++, via
    // wpa_supplicant1_proxy interface.
    void PropertiesChanged(
        const std::map<std::string, ::DBus::Variant>& properties) override;

    // We use a bare pointer, because each SupplicantBSSProxy is owned
    // (using a unique_ptr) by a WiFiEndpoint. This means that if
    // |wifi_endpoint_| is invalid, then so is |this|.
    WiFiEndpoint* wifi_endpoint_;
    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(SupplicantBSSProxy);
};

}  // namespace shill

#endif  // SHILL_SUPPLICANT_SUPPLICANT_BSS_PROXY_H_
