// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_dbus_control.h"

#include "shill/dbus/chromeos_device_dbus_adaptor.h"
#include "shill/dbus/chromeos_ipconfig_dbus_adaptor.h"
#include "shill/dbus/chromeos_manager_dbus_adaptor.h"
#include "shill/dbus/chromeos_profile_dbus_adaptor.h"
#include "shill/dbus/chromeos_rpc_task_dbus_adaptor.h"
#include "shill/dbus/chromeos_service_dbus_adaptor.h"
#include "shill/dbus/chromeos_third_party_vpn_dbus_adaptor.h"

using chromeos::dbus_utils::ExportedObjectManager;
using std::string;

namespace shill {

ChromeosDBusControl::ChromeosDBusControl(
    const base::WeakPtr<ExportedObjectManager>& object_manager,
    const scoped_refptr<dbus::Bus>& bus)
    : object_manager_(object_manager),
      bus_(bus) {}

ChromeosDBusControl::~ChromeosDBusControl() {}

template <typename Object, typename AdaptorInterface, typename Adaptor>
AdaptorInterface* ChromeosDBusControl::CreateAdaptor(Object* object) {
  return new Adaptor(object_manager_, bus_, object);
}

DeviceAdaptorInterface* ChromeosDBusControl::CreateDeviceAdaptor(
    Device* device) {
  return
      CreateAdaptor<Device, DeviceAdaptorInterface, ChromeosDeviceDBusAdaptor>(
          device);
}

IPConfigAdaptorInterface* ChromeosDBusControl::CreateIPConfigAdaptor(
    IPConfig* config) {
  return
      CreateAdaptor<IPConfig, IPConfigAdaptorInterface,
                    ChromeosIPConfigDBusAdaptor>(config);
}

ManagerAdaptorInterface* ChromeosDBusControl::CreateManagerAdaptor(
    Manager* manager) {
  return
      CreateAdaptor<Manager, ManagerAdaptorInterface,
                    ChromeosManagerDBusAdaptor>(manager);
}

ProfileAdaptorInterface* ChromeosDBusControl::CreateProfileAdaptor(
    Profile* profile) {
  return
      CreateAdaptor<Profile, ProfileAdaptorInterface,
                    ChromeosProfileDBusAdaptor>(profile);
}

RPCTaskAdaptorInterface* ChromeosDBusControl::CreateRPCTaskAdaptor(
    RPCTask* task) {
  return
      CreateAdaptor<RPCTask, RPCTaskAdaptorInterface,
                    ChromeosRPCTaskDBusAdaptor>(task);
}

ServiceAdaptorInterface* ChromeosDBusControl::CreateServiceAdaptor(
    Service* service) {
  return
      CreateAdaptor<Service, ServiceAdaptorInterface,
                    ChromeosServiceDBusAdaptor>(service);
}

#ifndef DISABLE_VPN
ThirdPartyVpnAdaptorInterface* ChromeosDBusControl::CreateThirdPartyVpnAdaptor(
    ThirdPartyVpnDriver* driver) {
  return
      CreateAdaptor<ThirdPartyVpnDriver, ThirdPartyVpnAdaptorInterface,
                    ChromeosThirdPartyVpnDBusAdaptor>(driver);
}
#endif

DBusPropertiesProxyInterface* ChromeosDBusControl::CreateDBusPropertiesProxy(
    const string& path,
    const string& service) {
  return nullptr;
}

DBusServiceProxyInterface* ChromeosDBusControl::CreateDBusServiceProxy() {
  return nullptr;
}

PowerManagerProxyInterface* ChromeosDBusControl::CreatePowerManagerProxy(
    PowerManagerProxyDelegate* delegate) {
  return nullptr;
}

#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
SupplicantProcessProxyInterface*
    ChromeosDBusControl::CreateSupplicantProcessProxy(
        const char* dbus_path,
        const char* dbus_addr) {
  return nullptr;
}

SupplicantInterfaceProxyInterface*
    ChromeosDBusControl::CreateSupplicantInterfaceProxy(
        SupplicantEventDelegateInterface* delegate,
        const string& object_path,
        const char* dbus_addr) {
  return nullptr;
}

SupplicantNetworkProxyInterface*
    ChromeosDBusControl::CreateSupplicantNetworkProxy(
        const string& object_path,
        const char* dbus_addr) {
  return nullptr;
}
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

#if !defined(DISABLE_WIFI)
SupplicantBSSProxyInterface* ChromeosDBusControl::CreateSupplicantBSSProxy(
    WiFiEndpoint* wifi_endpoint,
    const string& object_path,
    const char* dbus_addr) {
  return nullptr;
}
#endif  // DISABLE_WIFI

DHCPProxyInterface* ChromeosDBusControl::CreateDHCPProxy(
    const string& service) {
  return nullptr;
}

UpstartProxyInterface* ChromeosDBusControl::CreateUpstartProxy() {
  return nullptr;
}

PermissionBrokerProxyInterface*
    ChromeosDBusControl::CreatePermissionBrokerProxy() {
  return nullptr;
}

#if !defined(DISABLE_CELLULAR)
DBusObjectManagerProxyInterface*
    ChromeosDBusControl::CreateDBusObjectManagerProxy(
        const string& path,
        const string& service) {
  return nullptr;
}

ModemManagerProxyInterface*
    ChromeosDBusControl::CreateModemManagerProxy(
        ModemManagerClassic* manager,
        const string& path,
        const string& service) {
  return nullptr;
}

ModemProxyInterface* ChromeosDBusControl::CreateModemProxy(
    const string& path,
    const string& service) {
  return nullptr;
}

ModemSimpleProxyInterface* ChromeosDBusControl::CreateModemSimpleProxy(
    const string& path,
    const string& service) {
  return nullptr;
}

ModemCDMAProxyInterface* ChromeosDBusControl::CreateModemCDMAProxy(
    const string& path,
    const string& service) {
  return nullptr;
}

ModemGSMCardProxyInterface* ChromeosDBusControl::CreateModemGSMCardProxy(
    const string& path,
    const string& service) {
  return nullptr;
}

ModemGSMNetworkProxyInterface* ChromeosDBusControl::CreateModemGSMNetworkProxy(
    const string& path,
    const string& service) {
  return nullptr;
}

ModemGobiProxyInterface* ChromeosDBusControl::CreateModemGobiProxy(
    const string& path,
    const string& service) {
  return nullptr;
}

// Proxies for ModemManager1 interfaces
mm1::ModemModem3gppProxyInterface*
    ChromeosDBusControl::CreateMM1ModemModem3gppProxy(
        const string& path,
        const string& service) {
  return nullptr;
}

mm1::ModemModemCdmaProxyInterface*
    ChromeosDBusControl::CreateMM1ModemModemCdmaProxy(
        const string& path,
        const string& service) {
  return nullptr;
}

mm1::ModemProxyInterface* ChromeosDBusControl::CreateMM1ModemProxy(
    const string& path,
    const string& service) {
  return nullptr;
}

mm1::ModemSimpleProxyInterface* ChromeosDBusControl::CreateMM1ModemSimpleProxy(
    const string& path,
    const string& service) {
  return nullptr;
}

mm1::SimProxyInterface* ChromeosDBusControl::CreateSimProxy(
    const string& path,
    const string& service) {
  return nullptr;
}
#endif  // DISABLE_CELLULAR

#if !defined(DISABLE_WIMAX)
WiMaxDeviceProxyInterface* ChromeosDBusControl::CreateWiMaxDeviceProxy(
    const string& path) {
  return nullptr;
}

WiMaxManagerProxyInterface* ChromeosDBusControl::CreateWiMaxManagerProxy() {
  return nullptr;
}

WiMaxNetworkProxyInterface* ChromeosDBusControl::CreateWiMaxNetworkProxy(
    const string& path) {
  return nullptr;
}
#endif  // DISABLE_WIMAX

}  // namespace shill
