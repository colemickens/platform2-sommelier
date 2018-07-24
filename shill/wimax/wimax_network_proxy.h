// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
