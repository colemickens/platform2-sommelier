// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SUPPLICANT_NETWORK_PROXY_H_
#define SUPPLICANT_NETWORK_PROXY_H_

#include <map>
#include <string>

#include <base/basictypes.h>

#include "shill/dbus_bindings/supplicant-network.h"
#include "shill/supplicant_network_proxy_interface.h"
#include "shill/refptr_types.h"

namespace shill {

// SupplicantNetworkProxy. provides access to wpa_supplicant's
// network-interface APIs via D-Bus.
class SupplicantNetworkProxy
    : public SupplicantNetworkProxyInterface {
 public:
  SupplicantNetworkProxy(DBus::Connection *bus,
                         const ::DBus::Path &object_path,
                         const char *dbus_addr);
  virtual ~SupplicantNetworkProxy();

  virtual void SetEnabled(bool enabled);

 private:
  class Proxy : public fi::w1::wpa_supplicant1::Network_proxy,
    public ::DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *bus,
          const ::DBus::Path &object_path,
          const char *dbus_addr);
    virtual ~Proxy();

   private:
    // signal handlers called by dbus-c++, via
    // fi::w1::wpa_supplicant1::Network_proxy interface
    virtual void PropertiesChanged(const std::map<std::string, ::DBus::Variant>
                                   &properties);

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(SupplicantNetworkProxy);
};

}  // namespace shill

#endif  // SUPPLICANT_NETWORK_PROXY_H_
