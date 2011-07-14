// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus-c++/dbus.h>

#include "shill/supplicant_proxy_factory.h"

#include "shill/supplicant_interface_proxy.h"
#include "shill/supplicant_interface_proxy_interface.h"
#include "shill/supplicant_process_proxy.h"
#include "shill/supplicant_process_proxy_interface.h"

namespace shill {

SupplicantProxyFactory::SupplicantProxyFactory() {}

SupplicantProxyFactory::~SupplicantProxyFactory() {}

SupplicantProcessProxyInterface *
SupplicantProxyFactory::CreateProcessProxy(const char *dbus_path,
                                           const char *dbus_addr) {
  return new SupplicantProcessProxy(dbus_path, dbus_addr);
}

SupplicantInterfaceProxyInterface *
SupplicantProxyFactory::CreateInterfaceProxy(
    const WiFiRefPtr &wifi, const ::DBus::Path &object_path,
    const char *dbus_addr) {
  return new SupplicantInterfaceProxy(wifi, object_path, dbus_addr);
}

}  // namespace shill
