// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/proxy_factory.h"

#include "shill/dbus_properties_proxy.h"
#include "shill/dbus_service_proxy.h"
#include "shill/dhcpcd_proxy.h"
#include "shill/logging.h"
#include "shill/power_manager_proxy.h"
#include "shill/supplicant_bss_proxy.h"
#include "shill/supplicant_interface_proxy.h"
#include "shill/supplicant_network_proxy.h"
#include "shill/supplicant_process_proxy.h"

#if !defined(DISABLE_CELLULAR)
#include "shill/cellular/dbus_objectmanager_proxy.h"
#include "shill/cellular/mm1_bearer_proxy.h"
#include "shill/cellular/mm1_modem_location_proxy.h"
#include "shill/cellular/mm1_modem_modem3gpp_proxy.h"
#include "shill/cellular/mm1_modem_modemcdma_proxy.h"
#include "shill/cellular/mm1_modem_proxy.h"
#include "shill/cellular/mm1_modem_simple_proxy.h"
#include "shill/cellular/mm1_modem_time_proxy.h"
#include "shill/cellular/mm1_sim_proxy.h"
#include "shill/cellular/modem_cdma_proxy.h"
#include "shill/cellular/modem_gobi_proxy.h"
#include "shill/cellular/modem_gsm_card_proxy.h"
#include "shill/cellular/modem_gsm_network_proxy.h"
#include "shill/cellular/modem_manager_proxy.h"
#include "shill/cellular/modem_proxy.h"
#include "shill/cellular/modem_simple_proxy.h"
#endif

#if !defined(DISABLE_WIMAX)
#include "shill/wimax/wimax_device_proxy.h"
#include "shill/wimax/wimax_manager_proxy.h"
#include "shill/wimax/wimax_network_proxy.h"
#endif

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

DBusPropertiesProxyInterface *ProxyFactory::CreateDBusPropertiesProxy(
    const string &path,
    const string &service) {
  return new DBusPropertiesProxy(connection(), path, service);
}

DBusServiceProxyInterface *ProxyFactory::CreateDBusServiceProxy() {
  return new DBusServiceProxy(connection());
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
    SupplicantEventDelegateInterface *delegate,
    const DBus::Path &object_path,
    const char *dbus_addr) {
  return new SupplicantInterfaceProxy(delegate,
                                      connection(),
                                      object_path,
                                      dbus_addr);
}

SupplicantNetworkProxyInterface *ProxyFactory::CreateSupplicantNetworkProxy(
    const DBus::Path &object_path,
    const char *dbus_addr) {
  return new SupplicantNetworkProxy(connection(),
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

#if !defined(DISABLE_CELLULAR)

DBusObjectManagerProxyInterface *ProxyFactory::CreateDBusObjectManagerProxy(
    const string &path,
    const string &service) {
  return new DBusObjectManagerProxy(connection(), path, service);
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

ModemGobiProxyInterface *ProxyFactory::CreateModemGobiProxy(
    const string &path,
    const string &service) {
  return new ModemGobiProxy(connection(), path, service);
}

// Proxies for ModemManager1 interfaces
mm1::ModemModem3gppProxyInterface *ProxyFactory::CreateMM1ModemModem3gppProxy(
      const string &path,
      const string &service) {
  return new mm1::ModemModem3gppProxy(connection(), path, service);
}

mm1::ModemModemCdmaProxyInterface *ProxyFactory::CreateMM1ModemModemCdmaProxy(
      const string &path,
      const string &service) {
  return new mm1::ModemModemCdmaProxy(connection(), path, service);
}

mm1::ModemProxyInterface *ProxyFactory::CreateMM1ModemProxy(
      const string &path,
      const string &service) {
  return new mm1::ModemProxy(connection(), path, service);
}

mm1::ModemSimpleProxyInterface *ProxyFactory::CreateMM1ModemSimpleProxy(
      const string &path,
      const string &service) {
  return new mm1::ModemSimpleProxy(connection(), path, service);
}

mm1::ModemTimeProxyInterface *ProxyFactory::CreateMM1ModemTimeProxy(
      const string &path,
      const string &service) {
  return new mm1::ModemTimeProxy(connection(), path, service);
}

mm1::ModemLocationProxyInterface *ProxyFactory::CreateMM1ModemLocationProxy(
      const string &path,
      const string &service) {
  return new mm1::ModemLocationProxy(connection(), path, service);
}

mm1::SimProxyInterface *ProxyFactory::CreateSimProxy(
      const string &path,
      const string &service) {
  return new mm1::SimProxy(connection(), path, service);
}

mm1::BearerProxyInterface *ProxyFactory::CreateBearerProxy(
      const string &path,
      const string &service) {
  return new mm1::BearerProxy(connection(), path, service);
}

#endif  // DISABLE_CELLULAR

#if !defined(DISABLE_WIMAX)

WiMaxDeviceProxyInterface *ProxyFactory::CreateWiMaxDeviceProxy(
    const string &path) {
  return new WiMaxDeviceProxy(connection(), path);
}

WiMaxManagerProxyInterface *ProxyFactory::CreateWiMaxManagerProxy() {
  return new WiMaxManagerProxy(connection());
}

WiMaxNetworkProxyInterface *ProxyFactory::CreateWiMaxNetworkProxy(
    const string &path) {
  return new WiMaxNetworkProxy(connection(), path);
}

#endif  // DISABLE_WIMAX

}  // namespace shill
