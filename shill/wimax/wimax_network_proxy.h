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

#ifndef SHILL_WIMAX_WIMAX_NETWORK_PROXY_H_
#define SHILL_WIMAX_WIMAX_NETWORK_PROXY_H_

#include <string>

#include <base/callback.h>

#include "shill/wimax/wimax_network_proxy_interface.h"
#include "wimax_manager/dbus_proxies/org.chromium.WiMaxManager.Network.h"

namespace shill {

class WiMaxNetworkProxy : public WiMaxNetworkProxyInterface {
 public:
  // Constructs a WiMaxManager.Network DBus object proxy at |path|.
  WiMaxNetworkProxy(DBus::Connection* connection,
                    const DBus::Path& path);
  ~WiMaxNetworkProxy() override;

  // Inherited from WiMaxNetwokProxyInterface.
  RpcIdentifier path() const override;
  void set_signal_strength_changed_callback(
      const SignalStrengthChangedCallback& callback) override;
  uint32_t Identifier(Error* error) override;
  std::string Name(Error* error) override;
  int Type(Error* error) override;
  int CINR(Error* error) override;
  int RSSI(Error* error) override;
  int SignalStrength(Error* error) override;

 private:
  class Proxy : public org::chromium::WiMaxManager::Network_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection, const DBus::Path& path);
    ~Proxy() override;

    void set_signal_strength_changed_callback(
        const SignalStrengthChangedCallback& callback);

   private:
    // Signal callbacks inherited from WiMaxManager::Network_proxy.
    void SignalStrengthChanged(const int32_t& signal_strength) override;

    // Method callbacks inherited from WiMaxManager::Network_proxy.
    // [None]

    SignalStrengthChangedCallback signal_strength_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  static void FromDBusError(const DBus::Error& dbus_error, Error* error);

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxNetworkProxy);
};

}  // namespace shill

#endif  // SHILL_WIMAX_WIMAX_NETWORK_PROXY_H_
