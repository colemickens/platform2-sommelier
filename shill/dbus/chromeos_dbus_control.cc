// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_dbus_control.h"

#include <memory>

#include <brillo/dbus/async_event_sequencer.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/dbus/chromeos_dbus_service_watcher.h"
#include "shill/dbus/chromeos_device_dbus_adaptor.h"
#include "shill/dbus/chromeos_dhcpcd_listener.h"
#include "shill/dbus/chromeos_dhcpcd_proxy.h"
#include "shill/dbus/chromeos_ipconfig_dbus_adaptor.h"
#include "shill/dbus/chromeos_manager_dbus_adaptor.h"
#include "shill/dbus/chromeos_power_manager_proxy.h"
#include "shill/dbus/chromeos_profile_dbus_adaptor.h"
#include "shill/dbus/chromeos_rpc_task_dbus_adaptor.h"
#include "shill/dbus/chromeos_service_dbus_adaptor.h"
#include "shill/dbus/chromeos_third_party_vpn_dbus_adaptor.h"
#include "shill/dbus/chromeos_upstart_proxy.h"

#if !defined(DISABLE_CELLULAR)
#include "shill/dbus/chromeos_dbus_objectmanager_proxy.h"
#include "shill/dbus/chromeos_dbus_properties_proxy.h"
#include "shill/dbus/chromeos_mm1_modem_location_proxy.h"
#include "shill/dbus/chromeos_mm1_modem_modem3gpp_proxy.h"
#include "shill/dbus/chromeos_mm1_modem_modemcdma_proxy.h"
#include "shill/dbus/chromeos_mm1_modem_proxy.h"
#include "shill/dbus/chromeos_mm1_modem_simple_proxy.h"
#include "shill/dbus/chromeos_mm1_sim_proxy.h"
#include "shill/dbus/chromeos_modem_cdma_proxy.h"
#include "shill/dbus/chromeos_modem_gobi_proxy.h"
#include "shill/dbus/chromeos_modem_gsm_card_proxy.h"
#include "shill/dbus/chromeos_modem_gsm_network_proxy.h"
#include "shill/dbus/chromeos_modem_proxy.h"
#include "shill/dbus/chromeos_modem_simple_proxy.h"
#endif  // DISABLE_CELLULAR

#if !defined(DISABLE_WIFI)
#include "shill/dbus/chromeos_supplicant_bss_proxy.h"
#endif  // DISABLE_WIFI

#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
#include "shill/dbus/chromeos_supplicant_interface_proxy.h"
#include "shill/dbus/chromeos_supplicant_network_proxy.h"
#include "shill/dbus/chromeos_supplicant_process_proxy.h"
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

#include "shill/manager.h"

using brillo::dbus_utils::AsyncEventSequencer;
using std::string;

namespace shill {

// static.
const char ChromeosDBusControl::kNullPath[] = "/";

ChromeosDBusControl::ChromeosDBusControl(EventDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      null_identifier_(kNullPath) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;

  adaptor_bus_ = new dbus::Bus(options);
  proxy_bus_ = new dbus::Bus(options);
  CHECK(adaptor_bus_->Connect());
  CHECK(proxy_bus_->Connect());
}

ChromeosDBusControl::~ChromeosDBusControl() {
  if (adaptor_bus_) {
    adaptor_bus_->ShutdownAndBlock();
  }
  if (proxy_bus_) {
    proxy_bus_->ShutdownAndBlock();
  }
}

const string& ChromeosDBusControl::NullRPCIdentifier() {
  return null_identifier_;
}

void ChromeosDBusControl::RegisterManagerObject(
    Manager* manager, const base::Closure& registration_done_callback) {
  registration_done_callback_ = registration_done_callback;
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  manager->RegisterAsync(
      base::Bind(
          &ChromeosDBusControl::OnDBusServiceRegistered,
          base::Unretained(this),
          sequencer->GetHandler("Manager.RegisterAsync() failed.", true)));
  sequencer->OnAllTasksCompletedCall({
      base::Bind(&ChromeosDBusControl::TakeServiceOwnership,
                 base::Unretained(this))
  });
}

void ChromeosDBusControl::OnDBusServiceRegistered(
    const base::Callback<void(bool)>& completion_action, bool success) {
  // The DBus control interface will take over the ownership of the DBus service
  // in this callback.  The daemon will crash if registration failed.
  completion_action.Run(success);

  // We can start the manager now that we have ownership of the D-Bus service.
  // Doing so earlier would allow the manager to emit signals before service
  // ownership was acquired.
  registration_done_callback_.Run();
}

void ChromeosDBusControl::TakeServiceOwnership(bool success) {
  // Success should always be true since we've said that failures are fatal.
  CHECK(success) << "Init of one or more objects has failed.";
  CHECK(adaptor_bus_->RequestOwnershipAndBlock(kFlimflamServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << kFlimflamServiceName;
}

std::unique_ptr<DeviceAdaptorInterface>
ChromeosDBusControl::CreateDeviceAdaptor(Device* device) {
  return std::make_unique<ChromeosDeviceDBusAdaptor>(adaptor_bus_, device);
}

std::unique_ptr<IPConfigAdaptorInterface>
ChromeosDBusControl::CreateIPConfigAdaptor(IPConfig* config) {
  return std::make_unique<ChromeosIPConfigDBusAdaptor>(adaptor_bus_, config);
}

std::unique_ptr<ManagerAdaptorInterface>
ChromeosDBusControl::CreateManagerAdaptor(Manager* manager) {
  return std::make_unique<ChromeosManagerDBusAdaptor>(
      adaptor_bus_, proxy_bus_, manager);
}

std::unique_ptr<ProfileAdaptorInterface>
ChromeosDBusControl::CreateProfileAdaptor(Profile* profile) {
  return std::make_unique<ChromeosProfileDBusAdaptor>(adaptor_bus_, profile);
}

std::unique_ptr<RPCTaskAdaptorInterface>
ChromeosDBusControl::CreateRPCTaskAdaptor(RPCTask* task) {
  return std::make_unique<ChromeosRPCTaskDBusAdaptor>(adaptor_bus_, task);
}

std::unique_ptr<ServiceAdaptorInterface>
ChromeosDBusControl::CreateServiceAdaptor(Service* service) {
  return std::make_unique<ChromeosServiceDBusAdaptor>(adaptor_bus_, service);
}

#ifndef DISABLE_VPN
std::unique_ptr<ThirdPartyVpnAdaptorInterface>
ChromeosDBusControl::CreateThirdPartyVpnAdaptor(ThirdPartyVpnDriver* driver) {
  return std::make_unique<ChromeosThirdPartyVpnDBusAdaptor>(adaptor_bus_,
                                                            driver);
}
#endif

std::unique_ptr<PowerManagerProxyInterface>
ChromeosDBusControl::CreatePowerManagerProxy(
    PowerManagerProxyDelegate* delegate,
    const base::Closure& service_appeared_callback,
    const base::Closure& service_vanished_callback) {
  return std::make_unique<ChromeosPowerManagerProxy>(dispatcher_,
                                                     proxy_bus_,
                                                     delegate,
                                                     service_appeared_callback,
                                                     service_vanished_callback);
}

#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
std::unique_ptr<SupplicantProcessProxyInterface>
ChromeosDBusControl::CreateSupplicantProcessProxy(
    const base::Closure& service_appeared_callback,
    const base::Closure& service_vanished_callback) {
  return std::make_unique<ChromeosSupplicantProcessProxy>(
      dispatcher_,
      proxy_bus_,
      service_appeared_callback,
      service_vanished_callback);
}

std::unique_ptr<SupplicantInterfaceProxyInterface>
ChromeosDBusControl::CreateSupplicantInterfaceProxy(
    SupplicantEventDelegateInterface* delegate, const string& object_path) {
  return std::make_unique<ChromeosSupplicantInterfaceProxy>(
      proxy_bus_, object_path, delegate);
}

std::unique_ptr<SupplicantNetworkProxyInterface>
ChromeosDBusControl::CreateSupplicantNetworkProxy(const string& object_path) {
  return std::make_unique<ChromeosSupplicantNetworkProxy>(proxy_bus_,
                                                          object_path);
}
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

#if !defined(DISABLE_WIFI)
std::unique_ptr<SupplicantBSSProxyInterface>
ChromeosDBusControl::CreateSupplicantBSSProxy(WiFiEndpoint* wifi_endpoint,
                                              const string& object_path) {
  return std::make_unique<ChromeosSupplicantBSSProxy>(
      proxy_bus_, object_path, wifi_endpoint);
}
#endif  // DISABLE_WIFI

std::unique_ptr<DHCPCDListenerInterface>
ChromeosDBusControl::CreateDHCPCDListener(DHCPProvider* provider) {
  return std::make_unique<ChromeosDHCPCDListener>(
      proxy_bus_, dispatcher_, provider);
}

std::unique_ptr<DHCPProxyInterface> ChromeosDBusControl::CreateDHCPProxy(
    const string& service) {
  return std::make_unique<ChromeosDHCPCDProxy>(proxy_bus_, service);
}

std::unique_ptr<UpstartProxyInterface>
ChromeosDBusControl::CreateUpstartProxy() {
  return std::make_unique<ChromeosUpstartProxy>(proxy_bus_);
}

#if !defined(DISABLE_CELLULAR)
std::unique_ptr<DBusPropertiesProxyInterface>
ChromeosDBusControl::CreateDBusPropertiesProxy(const string& path,
                                               const string& service) {
  return std::make_unique<ChromeosDBusPropertiesProxy>(
      proxy_bus_, path, service);
}

std::unique_ptr<DBusObjectManagerProxyInterface>
ChromeosDBusControl::CreateDBusObjectManagerProxy(
    const string& path,
    const string& service,
    const base::Closure& service_appeared_callback,
    const base::Closure& service_vanished_callback) {
  return std::make_unique<ChromeosDBusObjectManagerProxy>(
      dispatcher_,
      proxy_bus_,
      path,
      service,
      service_appeared_callback,
      service_vanished_callback);
}

std::unique_ptr<ModemProxyInterface> ChromeosDBusControl::CreateModemProxy(
    const string& path, const string& service) {
  return std::make_unique<ChromeosModemProxy>(proxy_bus_, path, service);
}

std::unique_ptr<ModemSimpleProxyInterface>
ChromeosDBusControl::CreateModemSimpleProxy(const string& path,
                                            const string& service) {
  return std::make_unique<ChromeosModemSimpleProxy>(proxy_bus_, path, service);
}

std::unique_ptr<ModemCdmaProxyInterface>
ChromeosDBusControl::CreateModemCdmaProxy(const string& path,
                                          const string& service) {
  return std::make_unique<ChromeosModemCdmaProxy>(proxy_bus_, path, service);
}

std::unique_ptr<ModemGsmCardProxyInterface>
ChromeosDBusControl::CreateModemGsmCardProxy(const string& path,
                                             const string& service) {
  return std::make_unique<ChromeosModemGsmCardProxy>(proxy_bus_, path, service);
}

std::unique_ptr<ModemGsmNetworkProxyInterface>
ChromeosDBusControl::CreateModemGsmNetworkProxy(const string& path,
                                                const string& service) {
  return std::make_unique<ChromeosModemGsmNetworkProxy>(
      proxy_bus_, path, service);
}

std::unique_ptr<ModemGobiProxyInterface>
ChromeosDBusControl::CreateModemGobiProxy(const string& path,
                                          const string& service) {
  return std::make_unique<ChromeosModemGobiProxy>(proxy_bus_, path, service);
}

// Proxies for ModemManager1 interfaces
std::unique_ptr<mm1::ModemLocationProxyInterface>
ChromeosDBusControl::CreateMM1ModemLocationProxy(const string& path,
                                                 const string& service) {
  return std::make_unique<mm1::ChromeosModemLocationProxy>(
      proxy_bus_, path, service);
}

std::unique_ptr<mm1::ModemModem3gppProxyInterface>
ChromeosDBusControl::CreateMM1ModemModem3gppProxy(const string& path,
                                                  const string& service) {
  return std::make_unique<mm1::ChromeosModemModem3gppProxy>(
      proxy_bus_, path, service);
}

std::unique_ptr<mm1::ModemModemCdmaProxyInterface>
ChromeosDBusControl::CreateMM1ModemModemCdmaProxy(const string& path,
                                                  const string& service) {
  return std::make_unique<mm1::ChromeosModemModemCdmaProxy>(
      proxy_bus_, path, service);
}

std::unique_ptr<mm1::ModemProxyInterface>
ChromeosDBusControl::CreateMM1ModemProxy(const string& path,
                                         const string& service) {
  return std::make_unique<mm1::ChromeosModemProxy>(proxy_bus_, path, service);
}

std::unique_ptr<mm1::ModemSimpleProxyInterface>
ChromeosDBusControl::CreateMM1ModemSimpleProxy(const string& path,
                                               const string& service) {
  return std::make_unique<mm1::ChromeosModemSimpleProxy>(
      proxy_bus_, path, service);
}

std::unique_ptr<mm1::SimProxyInterface> ChromeosDBusControl::CreateMM1SimProxy(
    const string& path, const string& service) {
  return std::make_unique<mm1::ChromeosSimProxy>(proxy_bus_, path, service);
}
#endif  // DISABLE_CELLULAR

}  // namespace shill
