//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
