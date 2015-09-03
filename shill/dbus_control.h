//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_DBUS_CONTROL_H_
#define SHILL_DBUS_CONTROL_H_

#include <memory>
#include <string>

#include "shill/control_interface.h"

namespace DBus {
class Connection;
}

namespace shill {
// This is the Interface for the control channel for Shill.
class DBusControl : public ControlInterface {
 public:
  DBusControl();
  ~DBusControl() override;

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

  DBusPropertiesProxyInterface* CreateDBusPropertiesProxy(
      const std::string& path,
      const std::string& service) override;

  DBusServiceProxyInterface* CreateDBusServiceProxy() override;

  // The caller retains ownership of 'delegate'.  It must not be deleted before
  // the proxy.
  PowerManagerProxyInterface* CreatePowerManagerProxy(
      PowerManagerProxyDelegate* delegate) override;

#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
  SupplicantProcessProxyInterface* CreateSupplicantProcessProxy(
      const char* dbus_path,
      const char* dbus_addr) override;

  SupplicantInterfaceProxyInterface* CreateSupplicantInterfaceProxy(
      SupplicantEventDelegateInterface* delegate,
      const std::string& object_path,
      const char* dbus_addr) override;

  SupplicantNetworkProxyInterface* CreateSupplicantNetworkProxy(
      const std::string& object_path,
      const char* dbus_addr) override;
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

#if !defined(DISABLE_WIFI)
  // See comment in supplicant_bss_proxy.h, about bare pointer.
  SupplicantBSSProxyInterface* CreateSupplicantBSSProxy(
      WiFiEndpoint* wifi_endpoint,
      const std::string& object_path,
      const char* dbus_addr) override;
#endif  // DISABLE_WIFI

  UpstartProxyInterface* CreateUpstartProxy() override;

  DHCPCDListenerInterface* CreateDHCPCDListener(
      DHCPProvider* provider) override;

  DHCPProxyInterface* CreateDHCPProxy(const std::string& service) override;

  PermissionBrokerProxyInterface* CreatePermissionBrokerProxy() override;

#if !defined(DISABLE_CELLULAR)

  DBusObjectManagerProxyInterface* CreateDBusObjectManagerProxy(
      const std::string& path,
      const std::string& service) override;

  ModemManagerProxyInterface* CreateModemManagerProxy(
      ModemManagerClassic* manager,
      const std::string& path,
      const std::string& service) override;

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
  WiMaxManagerProxyInterface* CreateWiMaxManagerProxy() override;
  WiMaxNetworkProxyInterface* CreateWiMaxNetworkProxy(
      const std::string& path) override;

#endif  // DISABLE_WIMAX

  void Init();

 private:
  template <typename Object, typename AdaptorInterface, typename Adaptor>
  AdaptorInterface* CreateAdaptor(Object* object);
  DBus::Connection* GetConnection() const;
  DBus::Connection* GetProxyConnection() const;
};

}  // namespace shill

#endif  // SHILL_DBUS_CONTROL_H_
