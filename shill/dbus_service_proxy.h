// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_SERVICE_PROXY_H_
#define SHILL_DBUS_SERVICE_PROXY_H_

#include "shill/dbus_bindings/dbus-service.h"
#include "shill/dbus_service_proxy_interface.h"

namespace shill {

class DBusServiceProxy : public DBusServiceProxyInterface {
 public:
  explicit DBusServiceProxy(DBus::Connection *connection);
  virtual ~DBusServiceProxy();

  // Inherited from DBusServiceProxyInterface.
  virtual void GetNameOwner(const std::string &name,
                            Error *error,
                            const StringCallback &callback,
                            int timeout);
  virtual void set_name_owner_changed_callback(
      const NameOwnerChangedCallback &callback);

 private:
  class Proxy : public org::freedesktop::DBus_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection);
    virtual ~Proxy();

    void set_name_owner_changed_callback(
        const NameOwnerChangedCallback &callback);

   private:
    // Signal callbacks inherited from DBus_proxy.
    virtual void NameOwnerChanged(const std::string &name,
                                  const std::string &old_owner,
                                  const std::string &new_owner);

    // Method callbacks inherited from Device_proxy.
    virtual void GetNameOwnerCallback(const std::string &unique_name,
                                      const DBus::Error &error, void *data);

    NameOwnerChangedCallback name_owner_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  static void FromDBusError(const DBus::Error &dbus_error, Error *error);

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(DBusServiceProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_SERVICE_PROXY_H_
