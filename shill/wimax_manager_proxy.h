// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_MANAGER_PROXY_H_
#define SHILL_WIMAX_MANAGER_PROXY_H_

#include <vector>

#include <base/basictypes.h>

#include "shill/dbus_bindings/wimax_manager.h"
#include "shill/wimax_manager_proxy_interface.h"

namespace shill {

class WiMaxManagerProxy : public WiMaxManagerProxyInterface {
 public:
  WiMaxManagerProxy(DBus::Connection *connection);
  virtual ~WiMaxManagerProxy();

  // Inherited from WiMaxManagerProxyInterface.
  virtual std::vector<RpcIdentifier> Devices(Error *error);

 private:
  class Proxy : public org::chromium::WiMaxManager_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from WiMaxManager_proxy.
    // [None]

    // Method callbacks inherited from WiMaxManager_proxy.
    // [None]

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxManagerProxy);
};

}  // namespace shill

#endif  // SHILL_WIMAX_MANAGER_PROXY_H_
