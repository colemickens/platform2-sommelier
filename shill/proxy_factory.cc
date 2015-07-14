// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/proxy_factory.h"

#include "shill/dbus_properties_proxy.h"
#include "shill/dbus_service_proxy.h"
#include "shill/dhcp/dhcpcd_proxy.h"
#include "shill/logging.h"
#include "shill/permission_broker_proxy.h"
#include "shill/power_manager_proxy.h"
#include "shill/shared_dbus_connection.h"
#include "shill/upstart/upstart_proxy.h"

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

#if !defined(DISABLE_WIFI)
#include "shill/supplicant/supplicant_bss_proxy.h"
#endif  // DISABLE_WIFI

#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
#include "shill/supplicant/supplicant_interface_proxy.h"
#include "shill/supplicant/supplicant_network_proxy.h"
#include "shill/supplicant/supplicant_process_proxy.h"
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

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

ProxyFactory* ProxyFactory::GetInstance() {
  return g_proxy_factory.Pointer();
}

DBus::Connection* ProxyFactory::GetConnection() const {
  return SharedDBusConnection::GetInstance()->GetProxyConnection();
}

DBusPropertiesProxyInterface* ProxyFactory::CreateDBusPropertiesProxy(
    const string& path,
    const string& service) {
  return new DBusPropertiesProxy(GetConnection(), path, service);
}

DBusServiceProxyInterface* ProxyFactory::CreateDBusServiceProxy() {
  return new DBusServiceProxy(GetConnection());
}

PowerManagerProxyInterface* ProxyFactory::CreatePowerManagerProxy(
    PowerManagerProxyDelegate* delegate) {
  return new PowerManagerProxy(delegate, GetConnection());
}

#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
SupplicantProcessProxyInterface* ProxyFactory::CreateSupplicantProcessProxy(
    const char* dbus_path,
    const char* dbus_addr) {
  return new SupplicantProcessProxy(GetConnection(), dbus_path, dbus_addr);
}

SupplicantInterfaceProxyInterface* ProxyFactory::CreateSupplicantInterfaceProxy(
    SupplicantEventDelegateInterface* delegate,
    const string& object_path,
    const char* dbus_addr) {
  return new SupplicantInterfaceProxy(delegate,
                                      GetConnection(),
                                      object_path,
                                      dbus_addr);
}

SupplicantNetworkProxyInterface* ProxyFactory::CreateSupplicantNetworkProxy(
    const string& object_path,
    const char* dbus_addr) {
  return new SupplicantNetworkProxy(GetConnection(),
                                    object_path,
                                    dbus_addr);
}
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

#if !defined(DISABLE_WIFI)
SupplicantBSSProxyInterface* ProxyFactory::CreateSupplicantBSSProxy(
    WiFiEndpoint* wifi_endpoint,
    const string& object_path,
    const char* dbus_addr) {
  return new SupplicantBSSProxy(
      wifi_endpoint, GetConnection(), object_path, dbus_addr);
}
#endif  // DISABLE_WIFI

DHCPProxyInterface* ProxyFactory::CreateDHCPProxy(const string& service) {
  return new DHCPCDProxy(GetConnection(), service);
}

UpstartProxyInterface* ProxyFactory::CreateUpstartProxy() {
  return new UpstartProxy(GetConnection());
}

PermissionBrokerProxyInterface* ProxyFactory::CreatePermissionBrokerProxy() {
  return new PermissionBrokerProxy(GetConnection());
}

#if !defined(DISABLE_CELLULAR)

DBusObjectManagerProxyInterface* ProxyFactory::CreateDBusObjectManagerProxy(
    const string& path,
    const string& service) {
  return new DBusObjectManagerProxy(GetConnection(), path, service);
}

ModemManagerProxyInterface* ProxyFactory::CreateModemManagerProxy(
    ModemManagerClassic* manager,
    const string& path,
    const string& service) {
  return new ModemManagerProxy(GetConnection(), manager, path, service);
}

ModemProxyInterface* ProxyFactory::CreateModemProxy(
    const string& path,
    const string& service) {
  return new ModemProxy(GetConnection(), path, service);
}

ModemSimpleProxyInterface* ProxyFactory::CreateModemSimpleProxy(
    const string& path,
    const string& service) {
  return new ModemSimpleProxy(GetConnection(), path, service);
}

ModemCDMAProxyInterface* ProxyFactory::CreateModemCDMAProxy(
    const string& path,
    const string& service) {
  return new ModemCDMAProxy(GetConnection(), path, service);
}

ModemGSMCardProxyInterface* ProxyFactory::CreateModemGSMCardProxy(
    const string& path,
    const string& service) {
  return new ModemGSMCardProxy(GetConnection(), path, service);
}

ModemGSMNetworkProxyInterface* ProxyFactory::CreateModemGSMNetworkProxy(
    const string& path,
    const string& service) {
  return new ModemGSMNetworkProxy(GetConnection(), path, service);
}

ModemGobiProxyInterface* ProxyFactory::CreateModemGobiProxy(
    const string& path,
    const string& service) {
  return new ModemGobiProxy(GetConnection(), path, service);
}

// Proxies for ModemManager1 interfaces
mm1::ModemModem3gppProxyInterface* ProxyFactory::CreateMM1ModemModem3gppProxy(
      const string& path,
      const string& service) {
  return new mm1::ModemModem3gppProxy(GetConnection(), path, service);
}

mm1::ModemModemCdmaProxyInterface* ProxyFactory::CreateMM1ModemModemCdmaProxy(
      const string& path,
      const string& service) {
  return new mm1::ModemModemCdmaProxy(GetConnection(), path, service);
}

mm1::ModemProxyInterface* ProxyFactory::CreateMM1ModemProxy(
      const string& path,
      const string& service) {
  return new mm1::ModemProxy(GetConnection(), path, service);
}

mm1::ModemSimpleProxyInterface* ProxyFactory::CreateMM1ModemSimpleProxy(
      const string& path,
      const string& service) {
  return new mm1::ModemSimpleProxy(GetConnection(), path, service);
}

mm1::ModemTimeProxyInterface* ProxyFactory::CreateMM1ModemTimeProxy(
      const string& path,
      const string& service) {
  return new mm1::ModemTimeProxy(GetConnection(), path, service);
}

mm1::ModemLocationProxyInterface* ProxyFactory::CreateMM1ModemLocationProxy(
      const string& path,
      const string& service) {
  return new mm1::ModemLocationProxy(GetConnection(), path, service);
}

mm1::SimProxyInterface* ProxyFactory::CreateSimProxy(
      const string& path,
      const string& service) {
  return new mm1::SimProxy(GetConnection(), path, service);
}

mm1::BearerProxyInterface* ProxyFactory::CreateBearerProxy(
      const string& path,
      const string& service) {
  return new mm1::BearerProxy(GetConnection(), path, service);
}

#endif  // DISABLE_CELLULAR

#if !defined(DISABLE_WIMAX)

WiMaxDeviceProxyInterface* ProxyFactory::CreateWiMaxDeviceProxy(
    const string& path) {
  return new WiMaxDeviceProxy(GetConnection(), path);
}

WiMaxManagerProxyInterface* ProxyFactory::CreateWiMaxManagerProxy() {
  return new WiMaxManagerProxy(GetConnection());
}

WiMaxNetworkProxyInterface* ProxyFactory::CreateWiMaxNetworkProxy(
    const string& path) {
  return new WiMaxNetworkProxy(GetConnection(), path);
}

#endif  // DISABLE_WIMAX

}  // namespace shill
