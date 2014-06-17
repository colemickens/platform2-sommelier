// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PROXY_FACTORY_H_
#define SHILL_MOCK_PROXY_FACTORY_H_

#include <string>

#include "shill/proxy_factory.h"

#include <gmock/gmock.h>

namespace shill {

class MockProxyFactory : public ProxyFactory {
 public:
  MockProxyFactory();
  virtual ~MockProxyFactory();

  MOCK_METHOD0(Init, void());

  MOCK_METHOD2(CreateDBusPropertiesProxy,
               DBusPropertiesProxyInterface *(const std::string &path,
                                              const std::string &service));
  MOCK_METHOD0(CreateDBusServiceProxy, DBusServiceProxyInterface *());
  MOCK_METHOD1(
      CreatePowerManagerProxy,
      PowerManagerProxyInterface *(PowerManagerProxyDelegate *delegate));
  MOCK_METHOD2(CreateSupplicantProcessProxy,
               SupplicantProcessProxyInterface *(const char *dbus_path,
                                                 const char *dbus_addr));
  MOCK_METHOD3(CreateSupplicantInterfaceProxy,
               SupplicantInterfaceProxyInterface *(
                   SupplicantEventDelegateInterface *delegate,
                   const DBus::Path &object_path,
                   const char *dbus_addr));
  MOCK_METHOD2(CreateSupplicantNetworkProxy,
               SupplicantNetworkProxyInterface *(const DBus::Path &object_path,
                                                 const char *dbus_addr));
  MOCK_METHOD3(CreateSupplicantBSSProxy,
               SupplicantBSSProxyInterface *(WiFiEndpoint *wifi_endpoint,
                                             const DBus::Path &object_path,
                                             const char *dbus_addr));
  MOCK_METHOD1(CreateDHCPProxy,
               DHCPProxyInterface *(const std::string &service));

#if !defined(DISABLE_CELLULAR)

  MOCK_METHOD2(CreateDBusObjectManagerProxy,
               DBusObjectManagerProxyInterface *(const std::string &path,
                                                 const std::string &service));
  MOCK_METHOD3(CreateModemManagerProxy,
               ModemManagerProxyInterface *(ModemManagerClassic *manager,
                                            const std::string &path,
                                            const std::string &service));
  MOCK_METHOD2(CreateModemProxy,
               ModemProxyInterface *(const std::string &path,
                                     const std::string &service));
  MOCK_METHOD2(CreateModemSimpleProxy,
               ModemSimpleProxyInterface *(const std::string &path,
                                           const std::string &service));
  MOCK_METHOD2(CreateModemCDMAProxy,
               ModemCDMAProxyInterface *(const std::string &path,
                                         const std::string &service));
  MOCK_METHOD2(CreateModemGSMCardProxy,
               ModemGSMCardProxyInterface *(const std::string &path,
                                            const std::string &service));
  MOCK_METHOD2(CreateModemGSMNetworkProxy,
               ModemGSMNetworkProxyInterface *(const std::string &path,
                                               const std::string &service));
  MOCK_METHOD2(CreateModemGobiProxy,
               ModemGobiProxyInterface *(const std::string &path,
                                         const std::string &service));
  MOCK_METHOD2(CreateMM1ModemModem3gppProxy,
               mm1::ModemModem3gppProxyInterface *(const std::string &path,
                                                   const std::string &service));
  MOCK_METHOD2(CreateMM1ModemModemCdmaProxy,
               mm1::ModemModemCdmaProxyInterface *(const std::string &path,
                                                   const std::string &service));
  MOCK_METHOD2(CreateMM1ModemProxy,
               mm1::ModemProxyInterface *(const std::string &path,
                                          const std::string &service));
  MOCK_METHOD2(CreateMM1ModemLocationProxy,
               mm1::ModemLocationProxyInterface *(const std::string &path,
                                                  const std::string &service));
  MOCK_METHOD2(CreateMM1ModemSimpleProxy,
               mm1::ModemSimpleProxyInterface *(const std::string &path,
                                                const std::string &service));
  MOCK_METHOD2(CreateMM1ModemTimeProxy,
               mm1::ModemTimeProxyInterface *(const std::string &path,
                                              const std::string &service));
  MOCK_METHOD2(CreateSimProxy,
               mm1::SimProxyInterface *(const std::string &path,
                                        const std::string &service));
  MOCK_METHOD2(CreateBearerProxy,
               mm1::BearerProxyInterface *(const std::string &path,
                                           const std::string &service));

#endif  // DISABLE_CELLULAR

#if !defined(DISABLE_WIMAX)

  MOCK_METHOD1(CreateWiMaxDeviceProxy,
               WiMaxDeviceProxyInterface *(const std::string &path));
  MOCK_METHOD0(CreateWiMaxManagerProxy, WiMaxManagerProxyInterface *());
  MOCK_METHOD1(CreateWiMaxNetworkProxy,
               WiMaxNetworkProxyInterface *(const std::string &path));

#endif  // DISABLE_WIMAX

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProxyFactory);
};

}  // namespace shill

#endif  // SHILL_MOCK_PROXY_FACTORY_H_
