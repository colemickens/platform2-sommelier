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

#ifndef SHILL_CELLULAR_MODEM_PROXY_H_
#define SHILL_CELLULAR_MODEM_PROXY_H_

#include <string>

#include <base/macros.h>

#include "dbus_proxies/org.freedesktop.ModemManager.Modem.h"
#include "shill/cellular/modem_proxy_interface.h"

namespace shill {

// A proxy to (old) ModemManager.Modem.
class ModemProxy : public ModemProxyInterface {
 public:
  // Constructs a ModemManager.Modem DBus object proxy at |path| owned by
  // |service|.
  ModemProxy(DBus::Connection* connection,
             const std::string& path,
             const std::string& service);
  ~ModemProxy() override;

  // Inherited from ModemProxyInterface.
  void Enable(bool enable, Error* error,
              const ResultCallback& callback, int timeout) override;
  void Disconnect(Error* error, const ResultCallback& callback,
                  int timeout) override;
  void GetModemInfo(Error* error, const ModemInfoCallback& callback,
                    int timeout) override;

  void set_state_changed_callback(
      const ModemStateChangedSignalCallback& callback) override;

 private:
  class Proxy : public org::freedesktop::ModemManager::Modem_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

    void set_state_changed_callback(
        const ModemStateChangedSignalCallback& callback);

   private:
    // Signal callbacks inherited from ModemManager::Modem_proxy.
    void StateChanged(const uint32_t& old,
                      const uint32_t &_new,
                      const uint32_t& reason) override;

    // Method callbacks inherited from ModemManager::Modem_proxy.
    void EnableCallback(const DBus::Error& dberror, void* data) override;
    void GetInfoCallback(const ModemHardwareInfo& info,
                         const DBus::Error& dberror, void* data) override;
    void DisconnectCallback(const DBus::Error& dberror, void* data) override;

    ModemStateChangedSignalCallback state_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_PROXY_H_
