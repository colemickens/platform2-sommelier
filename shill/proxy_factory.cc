// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/proxy_factory.h"

#include <base/logging.h>

#include "shill/dhcpcd_proxy.h"
#include "shill/modem_manager_proxy.h"
#include "shill/supplicant_interface_proxy.h"
#include "shill/supplicant_process_proxy.h"

namespace shill {

ProxyFactory *ProxyFactory::factory_ = NULL;

ProxyFactory::ProxyFactory() {}

ProxyFactory::~ProxyFactory() {}

void ProxyFactory::Init() {
  CHECK(DBus::default_dispatcher);  // Initialized in DBusControl::Init.
  CHECK(!connection_.get());
  connection_.reset(new DBus::Connection(DBus::Connection::SystemBus()));
}

ModemManagerProxyInterface *ProxyFactory::CreateModemManagerProxy(
    ModemManager *manager,
    const std::string &path,
    const std::string &service) {
  return new ModemManagerProxy(connection(), manager, path, service);
}

SupplicantProcessProxyInterface *ProxyFactory::CreateSupplicantProcessProxy(
    const char *dbus_path,
    const char *dbus_addr) {
  return new SupplicantProcessProxy(connection(), dbus_path, dbus_addr);
}

SupplicantInterfaceProxyInterface *ProxyFactory::CreateSupplicantInterfaceProxy(
    const WiFiRefPtr &wifi,
    const DBus::Path &object_path,
    const char *dbus_addr) {
  return new SupplicantInterfaceProxy(wifi,
                                      connection(),
                                      object_path,
                                      dbus_addr);
}

DHCPProxyInterface *ProxyFactory::CreateDHCPProxy(const char *service) {
  return new DHCPCDProxy(connection(), service);
}

}  // namespace shill
