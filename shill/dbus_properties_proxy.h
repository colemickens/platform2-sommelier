// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_PROPERTIES_PROXY_
#define SHILL_DBUS_PROPERTIES_PROXY_

#include <base/basictypes.h>

#include "shill/dbus_bindings/dbus-properties.h"
#include "shill/dbus_properties_proxy_interface.h"

namespace shill {

class DBusPropertiesProxy : public DBusPropertiesProxyInterface {
 public:
  DBusPropertiesProxy(DBusPropertiesProxyDelegate *delegate,
                      DBus::Connection *connection,
                      const std::string &path,
                      const std::string &service);
  virtual ~DBusPropertiesProxy();

  // Inherited from DBusPropertiesProxyInterface.
  virtual DBusPropertiesMap GetAll(const std::string &interface_name);

 private:
  class Proxy : public org::freedesktop::DBus::Properties_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBusPropertiesProxyDelegate *delegate,
          DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from DBusProperties_proxy.
    virtual void MmPropertiesChanged(const std::string &interface,
                                     const DBusPropertiesMap &properties);

    virtual void PropertiesChanged(
        const std::string &interface,
        const DBusPropertiesMap &changed_properties,
        const std::vector<std::string> &invalidated_properties);

    DBusPropertiesProxyDelegate *delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(DBusPropertiesProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_PROPERTIES_PROXY_
