// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MM1_BEARER_PROXY_H_
#define SHILL_CELLULAR_MM1_BEARER_PROXY_H_

#include <string>

#include "dbus_proxies/org.freedesktop.ModemManager1.Bearer.h"
#include "shill/cellular/mm1_bearer_proxy_interface.h"
#include "shill/dbus_properties.h"

namespace shill {
namespace mm1 {

// A proxy to org.freedesktop.ModemManager1.Bearer.
class BearerProxy : public BearerProxyInterface {
 public:
  // Constructs an org.freedesktop.ModemManager1.Bearer DBus object
  // proxy at |path| owned by |service|.
  BearerProxy(DBus::Connection* connection,
              const std::string& path,
              const std::string& service);

  ~BearerProxy() override;

  // Inherited methods from BearerProxyInterface
  void Connect(Error* error,
               const ResultCallback& callback,
               int timeout) override;
  void Disconnect(Error* error,
                  const ResultCallback& callback,
                  int timeout) override;

 private:
  class Proxy : public org::freedesktop::ModemManager1::Bearer_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

   private:
    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::BearerProxy
    void ConnectCallback(const ::DBus::Error& dberror,
                         void* data) override;
    void DisconnectCallback(const ::DBus::Error& dberror,
                            void* data) override;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(BearerProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_CELLULAR_MM1_BEARER_PROXY_H_
