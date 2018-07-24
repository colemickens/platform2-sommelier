// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_control.h"

#include <string>

#include <dbus-c++/dbus.h>

#include "shill/device_dbus_adaptor.h"
#include "shill/ipconfig_dbus_adaptor.h"
#include "shill/logging.h"
#include "shill/manager_dbus_adaptor.h"
#include "shill/profile_dbus_adaptor.h"
#include "shill/rpc_task_dbus_adaptor.h"
#include "shill/service_dbus_adaptor.h"
#include "shill/shared_dbus_connection.h"
#include "shill/vpn/third_party_vpn_dbus_adaptor.h"

#include "shill/dbus_properties_proxy.h"
#include "shill/dbus_service_proxy.h"
#include "shill/dhcp/dhcpcd_proxy.h"
#include "shill/permission_broker_proxy.h"
#include "shill/power_manager_proxy.h"
#include "shill/upstart/upstart_proxy.h"

#if !defined(DISABLE_CELLULAR)
#include "shill/cellular/dbus_objectmanager_proxy.h"
#include "shill/cellular/mm1_modem_modem3gpp_proxy.h"
#include "shill/cellular/mm1_modem_modemcdma_proxy.h"
#include "shill/cellular/mm1_modem_proxy.h"
#include "shill/cellular/mm1_modem_simple_proxy.h"
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

DBusControl::DBusControl() {}

DBusControl::~DBusControl() {}

template <typename Object, typename AdaptorInterface, typename Adaptor>
AdaptorInterface* DBusControl::CreateAdaptor(Object* object) {
  AdaptorInterface* adaptor = nullptr;
  try {
    adaptor = new Adaptor(GetConnection(), object);
  } catch(const DBus::ErrorObjectPathInUse& error) {
    LOG(FATAL) << error.message() << " (object path in use)";
  } catch(const DBus::ErrorNoMemory& error) {
    LOG(FATAL) << error.message() << " (no memory)";
  } catch(const DBus::Error& error) {
    LOG(FATAL) << error.message();
  }
  return adaptor;
}

DeviceAdaptorInterface* DBusControl::CreateDeviceAdaptor(Device* device) {
  return CreateAdaptor<Device, DeviceAdaptorInterface, DeviceDBusAdaptor>(
      device);
}

IPConfigAdaptorInterface* DBusControl::CreateIPConfigAdaptor(IPConfig* config) {
  return CreateAdaptor<IPConfig, IPConfigAdaptorInterface, IPConfigDBusAdaptor>(
      config);
}

ManagerAdaptorInterface* DBusControl::CreateManagerAdaptor(Manager* manager) {
  return CreateAdaptor<Manager, ManagerAdaptorInterface, ManagerDBusAdaptor>(
      manager);
}

ProfileAdaptorInterface* DBusControl::CreateProfileAdaptor(Profile* profile) {
  return CreateAdaptor<Profile, ProfileAdaptorInterface, ProfileDBusAdaptor>(
      profile);
}

RPCTaskAdaptorInterface* DBusControl::CreateRPCTaskAdaptor(RPCTask* task) {
  return CreateAdaptor<RPCTask, RPCTaskAdaptorInterface, RPCTaskDBusAdaptor>(
      task);
}

ServiceAdaptorInterface* DBusControl::CreateServiceAdaptor(Service* service) {
  return CreateAdaptor<Service, ServiceAdaptorInterface, ServiceDBusAdaptor>(
      service);
}

#ifndef DISABLE_VPN
ThirdPartyVpnAdaptorInterface* DBusControl::CreateThirdPartyVpnAdaptor(
    ThirdPartyVpnDriver* driver) {
  return CreateAdaptor<ThirdPartyVpnDriver, ThirdPartyVpnAdaptorInterface,
                       ThirdPartyVpnAdaptor>(driver);
}
#endif

DBusPropertiesProxyInterface* DBusControl::CreateDBusPropertiesProxy(
    const string& path,
    const string& service) {
  return new DBusPropertiesProxy(GetProxyConnection(), path, service);
}

DBusServiceProxyInterface* DBusControl::CreateDBusServiceProxy() {
  return new DBusServiceProxy(GetProxyConnection());
}

PowerManagerProxyInterface* DBusControl::CreatePowerManagerProxy(
    PowerManagerProxyDelegate* delegate) {
  return new PowerManagerProxy(delegate, GetProxyConnection());
}

#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
SupplicantProcessProxyInterface* DBusControl::CreateSupplicantProcessProxy(
    const char* dbus_path,
    const char* dbus_addr) {
  return new SupplicantProcessProxy(GetProxyConnection(), dbus_path, dbus_addr);
}

SupplicantInterfaceProxyInterface* DBusControl::CreateSupplicantInterfaceProxy(
    SupplicantEventDelegateInterface* delegate,
    const string& object_path,
    const char* dbus_addr) {
  return new SupplicantInterfaceProxy(delegate,
                                      GetProxyConnection(),
                                      object_path,
                                      dbus_addr);
}

SupplicantNetworkProxyInterface* DBusControl::CreateSupplicantNetworkProxy(
    const string& object_path,
    const char* dbus_addr) {
  return new SupplicantNetworkProxy(GetProxyConnection(),
                                    object_path,
                                    dbus_addr);
}
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

#if !defined(DISABLE_WIFI)
SupplicantBSSProxyInterface* DBusControl::CreateSupplicantBSSProxy(
    WiFiEndpoint* wifi_endpoint,
    const string& object_path,
    const char* dbus_addr) {
  return new SupplicantBSSProxy(
      wifi_endpoint, GetProxyConnection(), object_path, dbus_addr);
}
#endif  // DISABLE_WIFI

DHCPCDListenerInterface* DBusControl::CreateDHCPCDListener(
    DHCPProvider* provider) {
  return new DHCPCDListener(GetProxyConnection(), provider);
}

DHCPProxyInterface* DBusControl::CreateDHCPProxy(const string& service) {
  return new DHCPCDProxy(GetProxyConnection(), service);
}

UpstartProxyInterface* DBusControl::CreateUpstartProxy() {
  return new UpstartProxy(GetProxyConnection());
}

PermissionBrokerProxyInterface* DBusControl::CreatePermissionBrokerProxy() {
  return new PermissionBrokerProxy(GetProxyConnection());
}

#if !defined(DISABLE_CELLULAR)
DBusObjectManagerProxyInterface* DBusControl::CreateDBusObjectManagerProxy(
    const string& path,
    const string& service) {
  return new DBusObjectManagerProxy(GetProxyConnection(), path, service);
}

ModemManagerProxyInterface* DBusControl::CreateModemManagerProxy(
    ModemManagerClassic* manager,
    const string& path,
    const string& service) {
  return new ModemManagerProxy(GetProxyConnection(), manager, path, service);
}

ModemProxyInterface* DBusControl::CreateModemProxy(
    const string& path,
    const string& service) {
  return new ModemProxy(GetProxyConnection(), path, service);
}

ModemSimpleProxyInterface* DBusControl::CreateModemSimpleProxy(
    const string& path,
    const string& service) {
  return new ModemSimpleProxy(GetProxyConnection(), path, service);
}

ModemCDMAProxyInterface* DBusControl::CreateModemCDMAProxy(
    const string& path,
    const string& service) {
  return new ModemCDMAProxy(GetProxyConnection(), path, service);
}

ModemGSMCardProxyInterface* DBusControl::CreateModemGSMCardProxy(
    const string& path,
    const string& service) {
  return new ModemGSMCardProxy(GetProxyConnection(), path, service);
}

ModemGSMNetworkProxyInterface* DBusControl::CreateModemGSMNetworkProxy(
    const string& path,
    const string& service) {
  return new ModemGSMNetworkProxy(GetProxyConnection(), path, service);
}

ModemGobiProxyInterface* DBusControl::CreateModemGobiProxy(
    const string& path,
    const string& service) {
  return new ModemGobiProxy(GetProxyConnection(), path, service);
}

// Proxies for ModemManager1 interfaces
mm1::ModemModem3gppProxyInterface* DBusControl::CreateMM1ModemModem3gppProxy(
      const string& path,
      const string& service) {
  return new mm1::ModemModem3gppProxy(GetProxyConnection(), path, service);
}

mm1::ModemModemCdmaProxyInterface* DBusControl::CreateMM1ModemModemCdmaProxy(
      const string& path,
      const string& service) {
  return new mm1::ModemModemCdmaProxy(GetProxyConnection(), path, service);
}

mm1::ModemProxyInterface* DBusControl::CreateMM1ModemProxy(
      const string& path,
      const string& service) {
  return new mm1::ModemProxy(GetProxyConnection(), path, service);
}

mm1::ModemSimpleProxyInterface* DBusControl::CreateMM1ModemSimpleProxy(
      const string& path,
      const string& service) {
  return new mm1::ModemSimpleProxy(GetProxyConnection(), path, service);
}

mm1::SimProxyInterface* DBusControl::CreateSimProxy(
      const string& path,
      const string& service) {
  return new mm1::SimProxy(GetProxyConnection(), path, service);
}
#endif  // DISABLE_CELLULAR

#if !defined(DISABLE_WIMAX)
WiMaxDeviceProxyInterface* DBusControl::CreateWiMaxDeviceProxy(
    const string& path) {
  return new WiMaxDeviceProxy(GetProxyConnection(), path);
}

WiMaxManagerProxyInterface* DBusControl::CreateWiMaxManagerProxy() {
  return new WiMaxManagerProxy(GetProxyConnection());
}

WiMaxNetworkProxyInterface* DBusControl::CreateWiMaxNetworkProxy(
    const string& path) {
  return new WiMaxNetworkProxy(GetProxyConnection(), path);
}
#endif  // DISABLE_WIMAX

void DBusControl::Init() {
  if (!GetConnection()->acquire_name(SHILL_INTERFACE)) {
    LOG(FATAL) << "Failed to acquire D-Bus name " << SHILL_INTERFACE << ". "
               << "Is another shill running?";
  }
}

DBus::Connection* DBusControl::GetConnection() const {
  return SharedDBusConnection::GetInstance()->GetAdaptorConnection();
}

DBus::Connection* DBusControl::GetProxyConnection() const {
  return SharedDBusConnection::GetInstance()->GetProxyConnection();
}

}  // namespace shill
