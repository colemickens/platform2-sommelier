// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SUPPLICANT_PROCESS_PROXY_INTERFACE_H_
#define SUPPLICANT_PROCESS_PROXY_INTERFACE_H_

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

namespace shill {

// SupplicantProcessProxyInterface declares only the subset of
// fi::w1::wpa_supplicant1_proxy that is actually used by WiFi.
class SupplicantProcessProxyInterface {
 public:
  virtual ~SupplicantProcessProxyInterface() {}
  virtual ::DBus::Path CreateInterface(
      const std::map<std::string, ::DBus::Variant> &args) = 0;
  virtual void RemoveInterface(const ::DBus::Path &path) = 0;
  virtual ::DBus::Path GetInterface(const std::string &ifname) = 0;
};

}  // namespace shill

#endif   // SUPPLICANT_PROCESS_PROXY_INTERFACE_H_
