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

#ifndef SHILL_WIMAX_WIMAX_MANAGER_PROXY_H_
#define SHILL_WIMAX_WIMAX_MANAGER_PROXY_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "shill/wimax/wimax_manager_proxy_interface.h"
#include "wimax_manager/dbus_proxies/org.chromium.WiMaxManager.h"

namespace shill {

class WiMaxManagerProxy : public WiMaxManagerProxyInterface {
 public:
  explicit WiMaxManagerProxy(DBus::Connection* connection);
  ~WiMaxManagerProxy() override;

  // Inherited from WiMaxManagerProxyInterface.
  void set_devices_changed_callback(
      const DevicesChangedCallback& callback) override;
  RpcIdentifiers Devices(Error* error) override;

 private:
  class Proxy : public org::chromium::WiMaxManager_proxy,
                public DBus::ObjectProxy {
   public:
    explicit Proxy(DBus::Connection* connection);
    ~Proxy() override;

    void set_devices_changed_callback(const DevicesChangedCallback& callback);

   private:
    // Signal callbacks inherited from WiMaxManager_proxy.
    void DevicesChanged(const std::vector<DBus::Path>& devices) override;

    // Method callbacks inherited from WiMaxManager_proxy.
    // [None]

    DevicesChangedCallback devices_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxManagerProxy);
};

}  // namespace shill

#endif  // SHILL_WIMAX_WIMAX_MANAGER_PROXY_H_
