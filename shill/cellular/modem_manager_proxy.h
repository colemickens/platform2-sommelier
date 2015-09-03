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

#ifndef SHILL_CELLULAR_MODEM_MANAGER_PROXY_H_
#define SHILL_CELLULAR_MODEM_MANAGER_PROXY_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "dbus_proxies/org.freedesktop.ModemManager.h"
#include "shill/cellular/modem_manager_proxy_interface.h"

namespace shill {

class ModemManagerClassic;

// There's a single proxy per (old) ModemManager service identified by
// its DBus |path| and owner name |service|.
class ModemManagerProxy : public ModemManagerProxyInterface {
 public:
  ModemManagerProxy(DBus::Connection* connection,
                    ModemManagerClassic* manager,
                    const std::string& path,
                    const std::string& service);
  ~ModemManagerProxy() override;

  // Inherited from ModemManagerProxyInterface.
  std::vector<DBus::Path> EnumerateDevices() override;

 private:
  class Proxy : public org::freedesktop::ModemManager_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          ModemManagerClassic* manager,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

   private:
    // Signal callbacks inherited from ModemManager_proxy.
    void DeviceAdded(const DBus::Path& device) override;
    void DeviceRemoved(const DBus::Path& device) override;

    // Method callbacks inherited from ModemManager_proxy.
    // [None]

    // The owner of this proxy.
    ModemManagerClassic* manager_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemManagerProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_MANAGER_PROXY_H_
