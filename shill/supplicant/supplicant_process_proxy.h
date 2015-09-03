//
// Copyright (C) 2011 The Android Open Source Project
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

#ifndef SHILL_SUPPLICANT_SUPPLICANT_PROCESS_PROXY_H_
#define SHILL_SUPPLICANT_SUPPLICANT_PROCESS_PROXY_H_

#include <map>
#include <string>

#include <base/macros.h>

#include "shill/dbus_proxies/supplicant-process.h"
#include "shill/supplicant/supplicant_process_proxy_interface.h"

namespace shill {

class SupplicantProcessProxy : public SupplicantProcessProxyInterface {
 public:
  SupplicantProcessProxy(DBus::Connection* bus,
                         const char* dbus_path,
                         const char* dbus_addr);
  ~SupplicantProcessProxy() override;

  bool CreateInterface(const KeyValueStore& args,
                       std::string* rpc_identifier) override;
  bool RemoveInterface(const std::string& rpc_identifier) override;
  bool GetInterface(const std::string& ifname,
                    std::string* rpc_identifier) override;
  bool SetDebugLevel(const std::string& level) override;
  bool GetDebugLevel(std::string* level) override;

 private:
  class Proxy : public fi::w1::wpa_supplicant1_proxy,
    public ::DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* bus, const char* dbus_path, const char* dbus_addr);
    ~Proxy() override;

   private:
    // signal handlers called by dbus-c++, via
    // wpa_supplicant1_proxy interface.
    void InterfaceAdded(
        const ::DBus::Path& path,
        const std::map<std::string, ::DBus::Variant>& properties) override;
    void InterfaceRemoved(const ::DBus::Path& path) override;
    void PropertiesChanged(
        const std::map<std::string, ::DBus::Variant>& properties) override;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(SupplicantProcessProxy);
};

}  // namespace shill

#endif  // SHILL_SUPPLICANT_SUPPLICANT_PROCESS_PROXY_H_
