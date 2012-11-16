// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_BEARER_PROXY_H_
#define SHILL_MM1_BEARER_PROXY_H_

#include <string>

#include "shill/dbus_bindings/mm1-bearer.h"
#include "shill/dbus_properties.h"
#include "shill/mm1_bearer_proxy_interface.h"

namespace shill {
namespace mm1 {

class BearerProxy : public BearerProxyInterface {
 public:
  // Constructs an org.freedesktop.ModemManager1.Bearer DBus object
  // proxy at |path| owned by |service|.
  BearerProxy(DBus::Connection *connection,
              const std::string &path,
              const std::string &service);

  virtual ~BearerProxy();

  // Inherited methods from BearerProxyInterface
  virtual void Connect(Error *error,
                       const ResultCallback &callback,
                       int timeout);
  virtual void Disconnect(Error *error,
                          const ResultCallback &callback,
                          int timeout);

  // Inherited properties from BearerProxyInterface
  virtual const std::string Interface();
  virtual bool Connected();
  virtual bool Suspended();
  virtual const DBusPropertiesMap Ip4Config();
  virtual const DBusPropertiesMap Ip6Config();
  virtual uint32_t IpTimeout();
  virtual const DBusPropertiesMap Properties();

 private:
  class Proxy : public org::freedesktop::ModemManager1::Bearer_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::BearerProxy
    virtual void ConnectCallback(const ::DBus::Error &dberror,
                                 void *data);
    virtual void DisconnectCallback(const ::DBus::Error &dberror,
                                    void *data);

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(BearerProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_BEARER_PROXY_H_
