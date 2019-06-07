// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CONTROL_H_
#define SHILL_MOCK_CONTROL_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/control_interface.h"
#include "shill/dhcp/dhcp_proxy_interface.h"
#include "shill/dhcp/dhcpcd_listener_interface.h"
#include "shill/power_manager_proxy_interface.h"
#include "shill/upstart/upstart_proxy_interface.h"

#if !defined(DISABLE_CELLULAR)
#include "shill/cellular/dbus_objectmanager_proxy_interface.h"
#include "shill/cellular/mm1_modem_location_proxy_interface.h"
#include "shill/cellular/mm1_modem_modem3gpp_proxy_interface.h"
#include "shill/cellular/mm1_modem_modemcdma_proxy_interface.h"
#include "shill/cellular/mm1_modem_proxy_interface.h"
#include "shill/cellular/mm1_modem_simple_proxy_interface.h"
#include "shill/cellular/mm1_sim_proxy_interface.h"
#include "shill/dbus_properties_proxy_interface.h"
#endif  // DISABLE_CELLULAR

#if !defined(DISABLE_WIFI)
#include "shill/supplicant/supplicant_bss_proxy_interface.h"
#endif  // DISABLE_WIFI

#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
#include "shill/supplicant/supplicant_interface_proxy_interface.h"
#include "shill/supplicant/supplicant_network_proxy_interface.h"
#include "shill/supplicant/supplicant_process_proxy_interface.h"
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

namespace shill {
// An implementation of the Shill RPC-channel-interface-factory interface that
// returns mocks.
class MockControl : public ControlInterface {
 public:
  MockControl();
  ~MockControl() override;

  void RegisterManagerObject(
      Manager* manager,
      const base::Closure& registration_done_callback) override {};

  // Each of these can be called once.  Ownership of the appropriate
  // interface pointer is given up upon call.
  std::unique_ptr<DeviceAdaptorInterface> CreateDeviceAdaptor(
      Device* device) override;
  std::unique_ptr<IPConfigAdaptorInterface> CreateIPConfigAdaptor(
      IPConfig* config) override;
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

  MOCK_METHOD3(CreatePowerManagerProxy,
               std::unique_ptr<PowerManagerProxyInterface>(
                   PowerManagerProxyDelegate* delegate,
                   const base::Closure& service_appeared_callback,
                   const base::Closure& service_vanished_callback));
#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
  MOCK_METHOD2(CreateSupplicantProcessProxy,
               std::unique_ptr<SupplicantProcessProxyInterface>(
                   const base::Closure& service_appeared_callback,
                   const base::Closure& service_vanished_callback));
  MOCK_METHOD2(CreateSupplicantInterfaceProxy,
               std::unique_ptr<SupplicantInterfaceProxyInterface>(
                   SupplicantEventDelegateInterface* delegate,
                   const std::string& object_path));
  MOCK_METHOD1(CreateSupplicantNetworkProxy,
               std::unique_ptr<SupplicantNetworkProxyInterface>(
                   const std::string& object_path));
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X
#if !defined(DISABLE_WIFI)
  MOCK_METHOD2(CreateSupplicantBSSProxy,
               std::unique_ptr<SupplicantBSSProxyInterface>(
                   WiFiEndpoint* wifi_endpoint,
                   const std::string& object_path));
#endif  // DISABLE_WIFI
  MOCK_METHOD1(
      CreateDHCPCDListener,
      std::unique_ptr<DHCPCDListenerInterface>(DHCPProvider* provider));
  MOCK_METHOD1(CreateDHCPProxy,
               std::unique_ptr<DHCPProxyInterface>(const std::string& service));

  MOCK_METHOD0(CreateUpstartProxy, std::unique_ptr<UpstartProxyInterface>());

#if !defined(DISABLE_CELLULAR)
  MOCK_METHOD2(CreateDBusPropertiesProxy,
               std::unique_ptr<DBusPropertiesProxyInterface>(
                   const std::string& path, const std::string& service));

  MOCK_METHOD4(CreateDBusObjectManagerProxy,
               std::unique_ptr<DBusObjectManagerProxyInterface>(
                   const std::string& path,
                   const std::string& service,
                   const base::Closure& service_appeared_callback,
                   const base::Closure& service_vanished_callback));
  MOCK_METHOD2(CreateMM1ModemLocationProxy,
               std::unique_ptr<mm1::ModemLocationProxyInterface>(
                   const std::string& path, const std::string& service));
  MOCK_METHOD2(CreateMM1ModemModem3gppProxy,
               std::unique_ptr<mm1::ModemModem3gppProxyInterface>(
                   const std::string& path, const std::string& service));
  MOCK_METHOD2(CreateMM1ModemModemCdmaProxy,
               std::unique_ptr<mm1::ModemModemCdmaProxyInterface>(
                   const std::string& path, const std::string& service));
  MOCK_METHOD2(CreateMM1ModemProxy,
               std::unique_ptr<mm1::ModemProxyInterface>(
                   const std::string& path, const std::string& service));
  MOCK_METHOD2(CreateMM1ModemSimpleProxy,
               std::unique_ptr<mm1::ModemSimpleProxyInterface>(
                   const std::string& path, const std::string& service));
  MOCK_METHOD2(CreateMM1SimProxy,
               std::unique_ptr<mm1::SimProxyInterface>(
                   const std::string& path, const std::string& service));
#endif  // DISABLE_CELLULAR

 private:
  std::string null_identifier_;

  DISALLOW_COPY_AND_ASSIGN(MockControl);
};

}  // namespace shill

#endif  // SHILL_MOCK_CONTROL_H_
