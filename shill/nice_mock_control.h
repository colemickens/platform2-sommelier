// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NICE_MOCK_CONTROL_H_
#define SHILL_NICE_MOCK_CONTROL_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/control_interface.h"

namespace shill {
// An implementation of the Shill RPC-channel-interface-factory interface that
// returns nice mocks.
class NiceMockControl : public ControlInterface {
 public:
  NiceMockControl();
  ~NiceMockControl() override;

  // Each of these can be called once.  Ownership of the appropriate
  // interface pointer is given up upon call.
  DeviceAdaptorInterface* CreateDeviceAdaptor(Device* device) override;
  IPConfigAdaptorInterface* CreateIPConfigAdaptor(IPConfig* config) override;
  ManagerAdaptorInterface* CreateManagerAdaptor(Manager* manager) override;
  ProfileAdaptorInterface* CreateProfileAdaptor(Profile* profile) override;
  RPCTaskAdaptorInterface* CreateRPCTaskAdaptor(RPCTask* task) override;
  ServiceAdaptorInterface* CreateServiceAdaptor(Service* service) override;
#ifndef DISABLE_VPN
  ThirdPartyVpnAdaptorInterface* CreateThirdPartyVpnAdaptor(
      ThirdPartyVpnDriver* driver) override;
#endif

  MOCK_METHOD2(CreateDBusPropertiesProxy,
               DBusPropertiesProxyInterface*(const std::string& path,
                                             const std::string& service));
  MOCK_METHOD0(CreateDBusServiceProxy, DBusServiceProxyInterface*());
  MOCK_METHOD1(
      CreatePowerManagerProxy,
      PowerManagerProxyInterface*(PowerManagerProxyDelegate* delegate));
#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
  MOCK_METHOD2(CreateSupplicantProcessProxy,
               SupplicantProcessProxyInterface*(const char* dbus_path,
                                                const char* dbus_addr));
  MOCK_METHOD3(CreateSupplicantInterfaceProxy,
               SupplicantInterfaceProxyInterface*(
                   SupplicantEventDelegateInterface* delegate,
                   const std::string& object_path,
                   const char* dbus_addr));
  MOCK_METHOD2(CreateSupplicantNetworkProxy,
               SupplicantNetworkProxyInterface*(const std::string& object_path,
                                                const char* dbus_addr));
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X
#if !defined(DISABLE_WIFI)
  MOCK_METHOD3(CreateSupplicantBSSProxy,
               SupplicantBSSProxyInterface*(WiFiEndpoint* wifi_endpoint,
                                            const std::string& object_path,
                                            const char* dbus_addr));
#endif  // DISABLE_WIFI
  MOCK_METHOD1(CreateDHCPProxy,
               DHCPProxyInterface*(const std::string& service));

  MOCK_METHOD0(CreateUpstartProxy, UpstartProxyInterface*());

  MOCK_METHOD0(CreatePermissionBrokerProxy, PermissionBrokerProxyInterface*());

#if !defined(DISABLE_CELLULAR)

  MOCK_METHOD2(CreateDBusObjectManagerProxy,
               DBusObjectManagerProxyInterface*(const std::string& path,
                                                const std::string& service));
  MOCK_METHOD3(CreateModemManagerProxy,
               ModemManagerProxyInterface*(ModemManagerClassic* manager,
                                           const std::string& path,
                                           const std::string& service));
  MOCK_METHOD2(CreateModemProxy,
               ModemProxyInterface*(const std::string& path,
                                    const std::string& service));
  MOCK_METHOD2(CreateModemSimpleProxy,
               ModemSimpleProxyInterface*(const std::string& path,
                                          const std::string& service));

  MOCK_METHOD2(CreateModemCDMAProxy,
               ModemCDMAProxyInterface*(const std::string& path,
                                        const std::string& service));
  MOCK_METHOD2(CreateModemGSMCardProxy,
               ModemGSMCardProxyInterface*(const std::string& path,
                                           const std::string& service));
  MOCK_METHOD2(CreateModemGSMNetworkProxy,
               ModemGSMNetworkProxyInterface*(const std::string& path,
                                              const std::string& service));
  MOCK_METHOD2(CreateModemGobiProxy,
               ModemGobiProxyInterface*(const std::string& path,
                                        const std::string& service));
  MOCK_METHOD2(CreateMM1ModemModem3gppProxy,
               mm1::ModemModem3gppProxyInterface*(const std::string& path,
                                                  const std::string& service));
  MOCK_METHOD2(CreateMM1ModemModemCdmaProxy,
               mm1::ModemModemCdmaProxyInterface*(const std::string& path,
                                                  const std::string& service));
  MOCK_METHOD2(CreateMM1ModemProxy,
               mm1::ModemProxyInterface*(const std::string& path,
                                         const std::string& service));
  MOCK_METHOD2(CreateMM1ModemSimpleProxy,
               mm1::ModemSimpleProxyInterface*(const std::string& path,
                                               const std::string& service));
  MOCK_METHOD2(CreateSimProxy,
               mm1::SimProxyInterface*(const std::string& path,
                                       const std::string& service));

#endif  // DISABLE_CELLULAR

#if !defined(DISABLE_WIMAX)

  MOCK_METHOD1(CreateWiMaxDeviceProxy,
               WiMaxDeviceProxyInterface*(const std::string& path));
  MOCK_METHOD0(CreateWiMaxManagerProxy, WiMaxManagerProxyInterface*());
  MOCK_METHOD1(CreateWiMaxNetworkProxy,
               WiMaxNetworkProxyInterface*(const std::string& path));

#endif  // DISABLE_WIMAX

 private:
  DISALLOW_COPY_AND_ASSIGN(NiceMockControl);
};

}  // namespace shill

#endif  // SHILL_NICE_MOCK_CONTROL_H_
