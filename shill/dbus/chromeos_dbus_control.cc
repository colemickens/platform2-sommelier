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

#include "shill/dbus/chromeos_dhcpcd_listener.h"
#include "shill/dbus/chromeos_dhcpcd_proxy.h"
#include "shill/dbus/chromeos_permission_broker_proxy.h"
#include "shill/dbus/chromeos_power_manager_proxy.h"
#include "shill/dbus/chromeos_supplicant_bss_proxy.h"
#include "shill/dbus/chromeos_supplicant_interface_proxy.h"
#include "shill/dbus/chromeos_supplicant_network_proxy.h"
#include "shill/dbus/chromeos_supplicant_process_proxy.h"
#include "shill/dbus/chromeos_upstart_proxy.h"

#include "shill/dbus/chromeos_dbus_service_watcher.h"

#if !defined(DISABLE_CELLULAR)
#include "shill/dbus/chromeos_dbus_objectmanager_proxy.h"
#include "shill/dbus/chromeos_dbus_properties_proxy.h"
#endif  // DISABLE_CELLULAR

#if !defined(DISABLE_WIMAX)
#include "shill/dbus/chromeos_wimax_device_proxy.h"
#include "shill/dbus/chromeos_wimax_manager_proxy.h"
#include "shill/dbus/chromeos_wimax_network_proxy.h"
#endif  // DISABLE_WIMAX

using chromeos::dbus_utils::ExportedObjectManager;
using std::string;

namespace shill {

// static.
const char ChromeosDBusControl::kNullPath[] = "/";

ChromeosDBusControl::ChromeosDBusControl(
    const scoped_refptr<dbus::Bus>& bus,
    EventDispatcher* dispatcher)
    : bus_(bus),
      dispatcher_(dispatcher),
      null_identifier_(kNullPath) {}

ChromeosDBusControl::~ChromeosDBusControl() {}

const string& ChromeosDBusControl::NullRPCIdentifier() {
  return null_identifier_;
}

template <typename Object, typename AdaptorInterface, typename Adaptor>
AdaptorInterface* ChromeosDBusControl::CreateAdaptor(Object* object) {
  return new Adaptor(bus_, object);
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

RPCServiceWatcherInterface* ChromeosDBusControl::CreateRPCServiceWatcher(
    const std::string& connection_name,
    const base::Closure& on_connection_vanished) {
  return new ChromeosDBusServiceWatcher(bus_,
                                        connection_name,
                                        on_connection_vanished);
}

DBusPropertiesProxyInterface* ChromeosDBusControl::CreateDBusPropertiesProxy(
    const string& path,
    const string& service) {
  return new ChromeosDBusPropertiesProxy(bus_, path, service);
}

DBusServiceProxyInterface* ChromeosDBusControl::CreateDBusServiceProxy() {
  return nullptr;
}

PowerManagerProxyInterface* ChromeosDBusControl::CreatePowerManagerProxy(
    PowerManagerProxyDelegate* delegate,
    const base::Closure& service_appeared_callback,
    const base::Closure& service_vanished_callback) {
  return new ChromeosPowerManagerProxy(dispatcher_,
                                       bus_,
                                       delegate,
                                       service_appeared_callback,
                                       service_vanished_callback);
}

#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
SupplicantProcessProxyInterface*
    ChromeosDBusControl::CreateSupplicantProcessProxy(
        const base::Closure& service_appeared_callback,
        const base::Closure& service_vanished_callback) {
  return new ChromeosSupplicantProcessProxy(
      dispatcher_, bus_, service_appeared_callback, service_vanished_callback);
}

SupplicantInterfaceProxyInterface*
    ChromeosDBusControl::CreateSupplicantInterfaceProxy(
        SupplicantEventDelegateInterface* delegate,
        const string& object_path) {
  return new ChromeosSupplicantInterfaceProxy(bus_, object_path, delegate);
}

SupplicantNetworkProxyInterface*
    ChromeosDBusControl::CreateSupplicantNetworkProxy(
        const string& object_path) {
  return new ChromeosSupplicantNetworkProxy(bus_, object_path);
}
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

#if !defined(DISABLE_WIFI)
SupplicantBSSProxyInterface* ChromeosDBusControl::CreateSupplicantBSSProxy(
    WiFiEndpoint* wifi_endpoint,
    const string& object_path) {
  return new ChromeosSupplicantBSSProxy(bus_, object_path, wifi_endpoint);
}
#endif  // DISABLE_WIFI

DHCPCDListenerInterface* ChromeosDBusControl::CreateDHCPCDListener(
    DHCPProvider* provider) {
  return new ChromeosDHCPCDListener(bus_, dispatcher_, provider);
}

DHCPProxyInterface* ChromeosDBusControl::CreateDHCPProxy(
    const string& service) {
  return new ChromeosDHCPCDProxy(bus_, service);
}

UpstartProxyInterface* ChromeosDBusControl::CreateUpstartProxy() {
  return new ChromeosUpstartProxy(bus_);
}

PermissionBrokerProxyInterface*
    ChromeosDBusControl::CreatePermissionBrokerProxy() {
  return new ChromeosPermissionBrokerProxy(bus_);
}

#if !defined(DISABLE_CELLULAR)
DBusObjectManagerProxyInterface*
    ChromeosDBusControl::CreateDBusObjectManagerProxy(
        const string& path,
        const string& service,
        const base::Closure& service_appeared_callback,
        const base::Closure& service_vanished_callback) {
  return new ChromeosDBusObjectManagerProxy(dispatcher_,
                                            bus_,
                                            path,
                                            service,
                                            service_appeared_callback,
                                            service_vanished_callback);
}

ModemManagerProxyInterface*
    ChromeosDBusControl::CreateModemManagerProxy(
        ModemManagerClassic* manager,
        const string& path,
        const string& service,
        const base::Closure& service_vanished_callback,
        const base::Closure& service_appeared_callback) {
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
  return new ChromeosWiMaxDeviceProxy(bus_, path);
}

WiMaxManagerProxyInterface* ChromeosDBusControl::CreateWiMaxManagerProxy(
    const base::Closure& service_appeared_callback,
    const base::Closure& service_vanished_callback) {
  return new ChromeosWiMaxManagerProxy(dispatcher_,
                                       bus_,
                                       service_appeared_callback,
                                       service_vanished_callback);
}

WiMaxNetworkProxyInterface* ChromeosDBusControl::CreateWiMaxNetworkProxy(
    const string& path) {
  return new ChromeosWiMaxNetworkProxy(bus_, path);
}
#endif  // DISABLE_WIMAX

}  // namespace shill
