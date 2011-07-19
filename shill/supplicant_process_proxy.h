// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SUPPLICANT_PROCESS_PROXY_H_
#define SUPPLICANT_PROCESS_PROXY_H_

#include <map>
#include <string>

#include <base/basictypes.h>

#include "shill/dbus_bindings/supplicant-process.h"
#include "shill/supplicant_process_proxy_interface.h"

namespace shill {

class SupplicantProcessProxy : public SupplicantProcessProxyInterface {
 public:
  SupplicantProcessProxy(DBus::Connection *bus,
                         const char *dbus_path,
                         const char *dbus_addr);
  virtual ~SupplicantProcessProxy();

  virtual ::DBus::Path CreateInterface(
      const std::map<std::string, ::DBus::Variant> &args);
  virtual void RemoveInterface(const ::DBus::Path &path);
  virtual ::DBus::Path GetInterface(const std::string &ifname);

 private:
  class Proxy : public fi::w1::wpa_supplicant1_proxy,
    public ::DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *bus, const char *dbus_path, const char *dbus_addr);
    virtual ~Proxy();

   private:
    // signal handlers called by dbus-c++, via
    // wpa_supplicant1_proxy interface.
    virtual void InterfaceAdded(
        const ::DBus::Path &path,
        const std::map<std::string, ::DBus::Variant> &properties);
    virtual void InterfaceRemoved(const ::DBus::Path &path);
    virtual void PropertiesChanged(
        const std::map<std::string, ::DBus::Variant> &properties);

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(SupplicantProcessProxy);

};

}  // namespace shill

#endif   // SUPPLICANT_PROCESS_PROXY_H_
