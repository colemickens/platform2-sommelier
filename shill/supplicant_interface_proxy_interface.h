// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SUPPLICANT_INTERFACE_PROXY_INTERFACE_H_
#define SUPPLICANT_INTERFACE_PROXY_INTERFACE_H_

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

namespace shill {

// SupplicantInterfaceProxyInterface declares only the subset of
// fi::w1::wpa_supplicant1::Interface_proxy that is actually used by WiFi.
class SupplicantInterfaceProxyInterface {
 public:
  virtual ~SupplicantInterfaceProxyInterface() {}

  virtual ::DBus::Path AddNetwork(
      const std::map<std::string, ::DBus::Variant> &args) = 0;
  virtual void ClearCachedCredentials() = 0;
  virtual void Disconnect() = 0;
  virtual void FlushBSS(const uint32_t &age) = 0;
  virtual void Reassociate() = 0;
  virtual void RemoveAllNetworks() = 0;
  virtual void RemoveNetwork(const ::DBus::Path &network) = 0;
  virtual void Scan(
      const std::map<std::string, ::DBus::Variant> &args) = 0;
  virtual void SelectNetwork(const ::DBus::Path &network) = 0;
  virtual void SetFastReauth(bool enabled) = 0;
  virtual void SetScanInterval(int seconds) = 0;
};

}  // namespace shill

#endif  // SUPPLICANT_INTERFACE_PROXY_INTERFACE_H_
