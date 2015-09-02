// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_DBUS_CONTROL_H_
#define SHILL_DBUS_CHROMEOS_DBUS_CONTROL_H_

#include <memory>
#include <string>

#include <chromeos/dbus/exported_object_manager.h>

#include "shill/control_interface.h"

namespace shill {

class EventDispatcher;

// This is the Interface for the  DBus control channel for Shill.
class ChromeosDBusControl : public ControlInterface {
 public:
  ChromeosDBusControl(const scoped_refptr<dbus::Bus>& bus,
                      EventDispatcher* dispatcher);
  ~ChromeosDBusControl() override;

  DeviceAdaptorInterface* CreateDeviceAdaptor(Device* device) override;
  IPConfigAdaptorInterface* CreateIPConfigAdaptor(IPConfig* ipconfig) override;
  ManagerAdaptorInterface* CreateManagerAdaptor(Manager* manager) override;
  ProfileAdaptorInterface* CreateProfileAdaptor(Profile* profile) override;
  RPCTaskAdaptorInterface* CreateRPCTaskAdaptor(RPCTask* task) override;
  ServiceAdaptorInterface* CreateServiceAdaptor(Service* service) override;
#ifndef DISABLE_VPN
  ThirdPartyVpnAdaptorInterface* CreateThirdPartyVpnAdaptor(
      ThirdPartyVpnDriver* driver) override;
#endif

  const std::string& NullRPCIdentifier() override;

  RPCServiceWatcherInterface* CreateRPCServiceWatcher(
      const std::string& connection_name,
      const base::Closure& on_connection_vanished) override;

  DBusServiceProxyInterface* CreateDBusServiceProxy() override;

  // The caller retains ownership of 'delegate'.  It must not be deleted before
  // the proxy.
  PowerManagerProxyInterface* CreatePowerManagerProxy(
      PowerManagerProxyDelegate* delegate,
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;

#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
  SupplicantProcessProxyInterface* CreateSupplicantProcessProxy(
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;

  SupplicantInterfaceProxyInterface* CreateSupplicantInterfaceProxy(
      SupplicantEventDelegateInterface* delegate,
      const std::string& object_path) override;

  SupplicantNetworkProxyInterface* CreateSupplicantNetworkProxy(
      const std::string& object_path) override;
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

#if !defined(DISABLE_WIFI)
  // See comment in supplicant_bss_proxy.h, about bare pointer.
  SupplicantBSSProxyInterface* CreateSupplicantBSSProxy(
      WiFiEndpoint* wifi_endpoint,
      const std::string& object_path) override;
#endif  // DISABLE_WIFI

  UpstartProxyInterface* CreateUpstartProxy() override;

  DHCPCDListenerInterface* CreateDHCPCDListener(
      DHCPProvider* provider) override;

  DHCPProxyInterface* CreateDHCPProxy(const std::string& service) override;

  FirewallProxyInterface* CreateFirewallProxy() override;

#if !defined(DISABLE_CELLULAR)
  DBusPropertiesProxyInterface* CreateDBusPropertiesProxy(
      const std::string& path,
      const std::string& service) override;

  DBusObjectManagerProxyInterface* CreateDBusObjectManagerProxy(
      const std::string& path,
      const std::string& service,
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;

  ModemManagerProxyInterface* CreateModemManagerProxy(
      ModemManagerClassic* manager,
      const std::string& path,
      const std::string& service,
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;

  ModemProxyInterface* CreateModemProxy(const std::string& path,
                                        const std::string& service) override;

  ModemSimpleProxyInterface* CreateModemSimpleProxy(
      const std::string& path,
      const std::string& service) override;

  ModemCDMAProxyInterface* CreateModemCDMAProxy(
      const std::string& path,
      const std::string& service) override;

  ModemGSMCardProxyInterface* CreateModemGSMCardProxy(
      const std::string& path,
      const std::string& service) override;

  ModemGSMNetworkProxyInterface* CreateModemGSMNetworkProxy(
      const std::string& path,
      const std::string& service) override;

  ModemGobiProxyInterface* CreateModemGobiProxy(
      const std::string& path,
      const std::string& service) override;

  // Proxies for ModemManager1 interfaces
  mm1::ModemModem3gppProxyInterface* CreateMM1ModemModem3gppProxy(
      const std::string& path,
      const std::string& service) override;

  mm1::ModemModemCdmaProxyInterface* CreateMM1ModemModemCdmaProxy(
      const std::string& path,
      const std::string& service) override;

  mm1::ModemProxyInterface* CreateMM1ModemProxy(
      const std::string& path,
      const std::string& service) override;

  mm1::ModemSimpleProxyInterface* CreateMM1ModemSimpleProxy(
      const std::string& path,
      const std::string& service) override;

  mm1::SimProxyInterface* CreateSimProxy(
      const std::string& path,
      const std::string& service) override;
#endif  // DISABLE_CELLULAR

#if !defined(DISABLE_WIMAX)
  WiMaxDeviceProxyInterface* CreateWiMaxDeviceProxy(
      const std::string& path) override;
  WiMaxManagerProxyInterface* CreateWiMaxManagerProxy(
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;
  WiMaxNetworkProxyInterface* CreateWiMaxNetworkProxy(
      const std::string& path) override;
#endif  // DISABLE_WIMAX

 private:
  template <typename Object, typename AdaptorInterface, typename Adaptor>
  AdaptorInterface* CreateAdaptor(Object* object);

  static const char kNullPath[];

  // Use separate bus connection for adaptors and proxies.  This allows the
  // proxy to receive all broadcast signal messages that it is interested in.
  // Refer to crbug.com/446837 for more info.
  scoped_refptr<dbus::Bus> adaptor_bus_;
  scoped_refptr<dbus::Bus> proxy_bus_;
  EventDispatcher* dispatcher_;
  std::string null_identifier_;
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_DBUS_CONTROL_H_
