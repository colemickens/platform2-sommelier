// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROXY_FACTORY_
#define SHILL_PROXY_FACTORY_

#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <dbus-c++/dbus.h>

#include "shill/refptr_types.h"

namespace shill {

class DBusPropertiesProxyInterface;
class DHCPProxyInterface;
class Modem;
class ModemManager;
class ModemManagerProxyInterface;
class SupplicantInterfaceProxyInterface;
class SupplicantProcessProxyInterface;

// Global DBus proxy factory that can be mocked out in tests.
class ProxyFactory {
 public:
  ProxyFactory();
  virtual ~ProxyFactory();

  void Init();

  virtual DBusPropertiesProxyInterface *CreateDBusPropertiesProxy(
      Modem *modem,
      const std::string &path,
      const std::string &service);

  virtual ModemManagerProxyInterface *CreateModemManagerProxy(
      ModemManager *manager,
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

  static ProxyFactory *factory() { return factory_; }
  static void set_factory(ProxyFactory *factory) { factory_ = factory; }

  DBus::Connection *connection() { return connection_.get(); }

 private:
  static ProxyFactory *factory_;

  scoped_ptr<DBus::Connection> connection_;

  DISALLOW_COPY_AND_ASSIGN(ProxyFactory);
};

}  // namespace shill

#endif  // SHILL_PROXY_FACTORY_
