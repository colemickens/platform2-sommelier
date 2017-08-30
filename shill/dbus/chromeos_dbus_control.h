//
// Copyright (C) 2015 The Android Open Source Project
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

#ifndef SHILL_DBUS_CHROMEOS_DBUS_CONTROL_H_
#define SHILL_DBUS_CHROMEOS_DBUS_CONTROL_H_

#include <string>

#include <brillo/dbus/exported_object_manager.h>

#include "shill/control_interface.h"

namespace shill {

class EventDispatcher;
class Manager;

class ChromeosDBusControl : public ControlInterface {
 public:
  explicit ChromeosDBusControl(EventDispatcher* dispatcher);
  ~ChromeosDBusControl() override;

  void RegisterManagerObject(
      Manager* manager,
      const base::Closure& registration_done_callback) override;
  std::unique_ptr<DeviceAdaptorInterface> CreateDeviceAdaptor(
      Device* device) override;
  std::unique_ptr<IPConfigAdaptorInterface> CreateIPConfigAdaptor(
      IPConfig* ipconfig) override;
  std::unique_ptr<ManagerAdaptorInterface> CreateManagerAdaptor(
      Manager* manager) override;
  std::unique_ptr<ProfileAdaptorInterface> CreateProfileAdaptor(
      Profile* profile) override;
  std::unique_ptr<RPCTaskAdaptorInterface> CreateRPCTaskAdaptor(
      RPCTask* task) override;
  std::unique_ptr<ServiceAdaptorInterface> CreateServiceAdaptor(
      Service* service) override;
#ifndef DISABLE_VPN
  std::unique_ptr<ThirdPartyVpnAdaptorInterface> CreateThirdPartyVpnAdaptor(
      ThirdPartyVpnDriver* driver) override;
#endif

  const std::string& NullRPCIdentifier() override;

  // The caller retains ownership of 'delegate'.  It must not be deleted before
  // the proxy.
  std::unique_ptr<PowerManagerProxyInterface> CreatePowerManagerProxy(
      PowerManagerProxyDelegate* delegate,
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;

#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
  std::unique_ptr<SupplicantProcessProxyInterface> CreateSupplicantProcessProxy(
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;

  std::unique_ptr<SupplicantInterfaceProxyInterface>
  CreateSupplicantInterfaceProxy(SupplicantEventDelegateInterface* delegate,
                                 const std::string& object_path) override;

  std::unique_ptr<SupplicantNetworkProxyInterface> CreateSupplicantNetworkProxy(
      const std::string& object_path) override;
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

#if !defined(DISABLE_WIFI)
  // See comment in supplicant_bss_proxy.h, about bare pointer.
  std::unique_ptr<SupplicantBSSProxyInterface> CreateSupplicantBSSProxy(
      WiFiEndpoint* wifi_endpoint, const std::string& object_path) override;
#endif  // DISABLE_WIFI

  std::unique_ptr<UpstartProxyInterface> CreateUpstartProxy() override;

  std::unique_ptr<DHCPCDListenerInterface> CreateDHCPCDListener(
      DHCPProvider* provider) override;

  std::unique_ptr<DHCPProxyInterface> CreateDHCPProxy(
      const std::string& service) override;

#if !defined(DISABLE_CELLULAR)
  std::unique_ptr<DBusPropertiesProxyInterface> CreateDBusPropertiesProxy(
      const std::string& path, const std::string& service) override;

  std::unique_ptr<DBusObjectManagerProxyInterface> CreateDBusObjectManagerProxy(
      const std::string& path,
      const std::string& service,
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;

  std::unique_ptr<ModemManagerProxyInterface> CreateModemManagerProxy(
      ModemManagerClassic* manager,
      const std::string& path,
      const std::string& service,
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;

  std::unique_ptr<ModemProxyInterface> CreateModemProxy(
      const std::string& path, const std::string& service) override;

  std::unique_ptr<ModemSimpleProxyInterface> CreateModemSimpleProxy(
      const std::string& path, const std::string& service) override;

  std::unique_ptr<ModemCDMAProxyInterface> CreateModemCDMAProxy(
      const std::string& path, const std::string& service) override;

  std::unique_ptr<ModemGSMCardProxyInterface> CreateModemGSMCardProxy(
      const std::string& path, const std::string& service) override;

  std::unique_ptr<ModemGSMNetworkProxyInterface> CreateModemGSMNetworkProxy(
      const std::string& path, const std::string& service) override;

  std::unique_ptr<ModemGobiProxyInterface> CreateModemGobiProxy(
      const std::string& path, const std::string& service) override;

  // Proxies for ModemManager1 interfaces
  std::unique_ptr<mm1::ModemLocationProxyInterface> CreateMM1ModemLocationProxy(
      const std::string& path, const std::string& service) override;

  std::unique_ptr<mm1::ModemModem3gppProxyInterface>
  CreateMM1ModemModem3gppProxy(const std::string& path,
                               const std::string& service) override;

  std::unique_ptr<mm1::ModemModemCdmaProxyInterface>
  CreateMM1ModemModemCdmaProxy(const std::string& path,
                               const std::string& service) override;

  std::unique_ptr<mm1::ModemProxyInterface> CreateMM1ModemProxy(
      const std::string& path, const std::string& service) override;

  std::unique_ptr<mm1::ModemSimpleProxyInterface> CreateMM1ModemSimpleProxy(
      const std::string& path, const std::string& service) override;

  std::unique_ptr<mm1::SimProxyInterface> CreateMM1SimProxy(
      const std::string& path, const std::string& service) override;
#endif  // DISABLE_CELLULAR

#if !defined(DISABLE_WIMAX)
  std::unique_ptr<WiMaxDeviceProxyInterface> CreateWiMaxDeviceProxy(
      const std::string& path) override;
  std::unique_ptr<WiMaxManagerProxyInterface> CreateWiMaxManagerProxy(
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;
  std::unique_ptr<WiMaxNetworkProxyInterface> CreateWiMaxNetworkProxy(
      const std::string& path) override;
#endif  // DISABLE_WIMAX

 private:
  template <typename Object, typename AdaptorInterface, typename Adaptor>
  std::unique_ptr<AdaptorInterface> CreateAdaptor(Object* object);

  void OnDBusServiceRegistered(
      const base::Callback<void(bool)>& completion_action, bool success);
  void TakeServiceOwnership(bool success);

  static const char kNullPath[];

  // Use separate bus connection for adaptors and proxies.  This allows the
  // proxy to receive all broadcast signal messages that it is interested in.
  // Refer to crbug.com/446837 for more info.
  scoped_refptr<dbus::Bus> adaptor_bus_;
  scoped_refptr<dbus::Bus> proxy_bus_;
  EventDispatcher* dispatcher_;
  std::string null_identifier_;
  base::Closure registration_done_callback_;
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_DBUS_CONTROL_H_
