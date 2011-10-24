// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROXY_FACTORY_
#define SHILL_PROXY_FACTORY_

#include <string>

#include <base/basictypes.h>
#include <base/lazy_instance.h>
#include <base/memory/scoped_ptr.h>
#include <dbus-c++/dbus.h>

#include "shill/refptr_types.h"

namespace shill {

class DBusPropertiesProxyDelegate;
class DBusPropertiesProxyInterface;
class DHCPProxyInterface;
class ModemCDMAProxyDelegate;
class ModemCDMAProxyInterface;
class ModemGSMCardProxyDelegate;
class ModemGSMCardProxyInterface;
class ModemGSMNetworkProxyDelegate;
class ModemGSMNetworkProxyInterface;
class ModemManager;
class ModemManagerProxyInterface;
class ModemProxyDelegate;
class ModemProxyInterface;
class ModemSimpleProxyInterface;
class SupplicantInterfaceProxyInterface;
class SupplicantProcessProxyInterface;

// Global DBus proxy factory that can be mocked out in tests.
class ProxyFactory {
 public:
  virtual ~ProxyFactory();

  // Since this is a singleton, use ProxyFactory::GetInstance()->Foo()
  static ProxyFactory *GetInstance();

  void Init();

  virtual DBusPropertiesProxyInterface *CreateDBusPropertiesProxy(
      DBusPropertiesProxyDelegate *delegate,
      const std::string &path,
      const std::string &service);

  virtual ModemManagerProxyInterface *CreateModemManagerProxy(
      ModemManager *manager,
      const std::string &path,
      const std::string &service);

  virtual ModemProxyInterface *CreateModemProxy(ModemProxyDelegate *delegate,
                                                const std::string &path,
                                                const std::string &service);

  virtual ModemSimpleProxyInterface *CreateModemSimpleProxy(
      const std::string &path,
      const std::string &service);

  virtual ModemCDMAProxyInterface *CreateModemCDMAProxy(
      ModemCDMAProxyDelegate *delegate,
      const std::string &path,
      const std::string &service);

  virtual ModemGSMCardProxyInterface *CreateModemGSMCardProxy(
      ModemGSMCardProxyDelegate *delegate,
      const std::string &path,
      const std::string &service);

  virtual ModemGSMNetworkProxyInterface *CreateModemGSMNetworkProxy(
      ModemGSMNetworkProxyDelegate *delegate,
      const std::string &path,
      const std::string &service);

  virtual SupplicantProcessProxyInterface *CreateSupplicantProcessProxy(
      const char *dbus_path,
      const char *dbus_addr);

  virtual SupplicantInterfaceProxyInterface *CreateSupplicantInterfaceProxy(
      const WiFiRefPtr &wifi,
      const DBus::Path &object_path,
      const char *dbus_addr);

  virtual DHCPProxyInterface *CreateDHCPProxy(const std::string &service);

  DBus::Connection *connection() const { return connection_.get(); }

 protected:
  ProxyFactory();

 private:
  friend struct base::DefaultLazyInstanceTraits<ProxyFactory>;

  scoped_ptr<DBus::Connection> connection_;

  DISALLOW_COPY_AND_ASSIGN(ProxyFactory);
};

}  // namespace shill

#endif  // SHILL_PROXY_FACTORY_
