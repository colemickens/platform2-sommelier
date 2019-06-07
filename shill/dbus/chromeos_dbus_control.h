// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_DBUS_CONTROL_H_
#define SHILL_DBUS_CHROMEOS_DBUS_CONTROL_H_

#include <memory>
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
  std::unique_ptr<RpcTaskAdaptorInterface> CreateRpcTaskAdaptor(
      RpcTask* task) override;
  std::unique_ptr<ServiceAdaptorInterface> CreateServiceAdaptor(
      Service* service) override;
#ifndef DISABLE_VPN
  std::unique_ptr<ThirdPartyVpnAdaptorInterface> CreateThirdPartyVpnAdaptor(
      ThirdPartyVpnDriver* driver) override;
#endif

  const std::string& NullRpcIdentifier() override;

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

 private:
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
