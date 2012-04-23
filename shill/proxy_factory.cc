// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/proxy_factory.h"

#include <base/logging.h>

#include "shill/dbus_objectmanager_proxy.h"
#include "shill/dbus_properties_proxy.h"
#include "shill/dhcpcd_proxy.h"
#include "shill/mm1_modem_modem3gpp_proxy.h"
#include "shill/mm1_modem_modemcdma_proxy.h"
#include "shill/mm1_modem_proxy.h"
#include "shill/mm1_modem_simple_proxy.h"
#include "shill/mm1_sim_proxy.h"
#include "shill/modem_cdma_proxy.h"
#include "shill/modem_gsm_card_proxy.h"
#include "shill/modem_gsm_network_proxy.h"
#include "shill/modem_manager_proxy.h"
#include "shill/modem_proxy.h"
#include "shill/modem_simple_proxy.h"
#include "shill/power_manager_proxy.h"
#include "shill/supplicant_bss_proxy.h"
#include "shill/supplicant_interface_proxy.h"
#include "shill/supplicant_process_proxy.h"

using std::string;

namespace shill {

namespace {
base::LazyInstance<ProxyFactory> g_proxy_factory = LAZY_INSTANCE_INITIALIZER;
}  // namespace

ProxyFactory::ProxyFactory() {}

ProxyFactory::~ProxyFactory() {}

ProxyFactory *ProxyFactory::GetInstance() {
  return g_proxy_factory.Pointer();
}

void ProxyFactory::Init() {
  CHECK(DBus::default_dispatcher);  // Initialized in DBusControl::Init.
  CHECK(!connection_.get());
  connection_.reset(new DBus::Connection(DBus::Connection::SystemBus()));
}

DBusObjectManagerProxyInterface *ProxyFactory::CreateDBusObjectManagerProxy(
    const string &path,
    const string &service) {
  return new DBusObjectManagerProxy(connection(), path, service);
}

DBusPropertiesProxyInterface *ProxyFactory::CreateDBusPropertiesProxy(
    DBusPropertiesProxyDelegate *delegate,
    const string &path,
    const string &service) {
  return new DBusPropertiesProxy(delegate, connection(), path, service);
}

ModemManagerProxyInterface *ProxyFactory::CreateModemManagerProxy(
    ModemManagerClassic *manager,
    const string &path,
    const string &service) {
  return new ModemManagerProxy(connection(), manager, path, service);
}

ModemProxyInterface *ProxyFactory::CreateModemProxy(
    const string &path,
    const string &service) {
  return new ModemProxy(connection(), path, service);
}

ModemSimpleProxyInterface *ProxyFactory::CreateModemSimpleProxy(
    const string &path,
    const string &service) {
  return new ModemSimpleProxy(connection(), path, service);
}

ModemCDMAProxyInterface *ProxyFactory::CreateModemCDMAProxy(
    const string &path,
    const string &service) {
  return new ModemCDMAProxy(connection(), path, service);
}

ModemGSMCardProxyInterface *ProxyFactory::CreateModemGSMCardProxy(
    const string &path,
    const string &service) {
  return new ModemGSMCardProxy(connection(), path, service);
}

ModemGSMNetworkProxyInterface *ProxyFactory::CreateModemGSMNetworkProxy(
    const string &path,
    const string &service) {
  return new ModemGSMNetworkProxy(connection(), path, service);
}

// Proxies for ModemManager1 interfaces
mm1::ModemModem3gppProxyInterface *ProxyFactory::CreateMM1ModemModem3gppProxy(
      const std::string &path,
      const std::string &service) {
  return new mm1::ModemModem3gppProxy(connection(), path, service);
}

mm1::ModemModemCdmaProxyInterface *ProxyFactory::CreateMM1ModemModemCdmaProxy(
      const std::string &path,
      const std::string &service) {
  return new mm1::ModemModemCdmaProxy(connection(), path, service);
}

mm1::ModemProxyInterface *ProxyFactory::CreateMM1ModemProxy(
      const std::string &path,
      const std::string &service) {
  return new mm1::ModemProxy(connection(), path, service);
}

mm1::ModemSimpleProxyInterface *ProxyFactory::CreateMM1ModemSimpleProxy(
      const std::string &path,
      const std::string &service) {
  return new mm1::ModemSimpleProxy(connection(), path, service);
}

mm1::SimProxyInterface *ProxyFactory::CreateSimProxy(
      const std::string &path,
      const std::string &service) {
  return new mm1::SimProxy(connection(), path, service);
}

PowerManagerProxyInterface *ProxyFactory::CreatePowerManagerProxy(
    PowerManagerProxyDelegate *delegate) {
  return new PowerManagerProxy(delegate, connection());
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

SupplicantBSSProxyInterface *ProxyFactory::CreateSupplicantBSSProxy(
    WiFiEndpoint *wifi_endpoint,
    const DBus::Path &object_path,
    const char *dbus_addr) {
  return new SupplicantBSSProxy(
      wifi_endpoint, connection(), object_path, dbus_addr);
}

DHCPProxyInterface *ProxyFactory::CreateDHCPProxy(const string &service) {
  return new DHCPCDProxy(connection(), service);
}

}  // namespace shill
