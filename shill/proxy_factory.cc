// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/proxy_factory.h"

#include <base/logging.h>

#include "shill/dbus_properties_proxy.h"
#include "shill/dhcpcd_proxy.h"
#include "shill/modem_cdma_proxy.h"
#include "shill/modem_gsm_network_proxy.h"
#include "shill/modem_manager_proxy.h"
#include "shill/modem_proxy.h"
#include "shill/modem_simple_proxy.h"
#include "shill/supplicant_interface_proxy.h"
#include "shill/supplicant_process_proxy.h"

using std::string;

namespace shill {

ProxyFactory *ProxyFactory::factory_ = NULL;

ProxyFactory::ProxyFactory() {}

ProxyFactory::~ProxyFactory() {}

void ProxyFactory::Init() {
  CHECK(DBus::default_dispatcher);  // Initialized in DBusControl::Init.
  CHECK(!connection_.get());
  connection_.reset(new DBus::Connection(DBus::Connection::SystemBus()));
}

DBusPropertiesProxyInterface *ProxyFactory::CreateDBusPropertiesProxy(
    DBusPropertiesProxyListener *listener,
    const string &path,
    const string &service) {
  return new DBusPropertiesProxy(listener, connection(), path, service);
}

ModemManagerProxyInterface *ProxyFactory::CreateModemManagerProxy(
    ModemManager *manager,
    const string &path,
    const string &service) {
  return new ModemManagerProxy(connection(), manager, path, service);
}

ModemProxyInterface *ProxyFactory::CreateModemProxy(
    ModemProxyListener *listener,
    const string &path,
    const string &service) {
  return new ModemProxy(listener, connection(), path, service);
}

ModemSimpleProxyInterface *ProxyFactory::CreateModemSimpleProxy(
    const string &path,
    const string &service) {
  return new ModemSimpleProxy(connection(), path, service);
}

ModemCDMAProxyInterface *ProxyFactory::CreateModemCDMAProxy(
    ModemCDMAProxyListener *listener,
    const string &path,
    const string &service) {
  return new ModemCDMAProxy(listener, connection(), path, service);
}

ModemGSMNetworkProxyInterface *ProxyFactory::CreateModemGSMNetworkProxy(
    ModemGSMNetworkProxyListener *listener,
    const string &path,
    const string &service) {
  return new ModemGSMNetworkProxy(listener, connection(), path, service);
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

DHCPProxyInterface *ProxyFactory::CreateDHCPProxy(const string &service) {
  return new DHCPCDProxy(connection(), service);
}

}  // namespace shill
