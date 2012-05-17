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
  explicit WiMaxManagerProxy(DBus::Connection *connection);
  virtual ~WiMaxManagerProxy();

  // Inherited from WiMaxManagerProxyInterface.
  virtual void set_devices_changed_callback(
      const DevicesChangedCallback &callback);
  virtual RpcIdentifiers Devices(Error *error);

 private:
  class Proxy : public org::chromium::WiMaxManager_proxy,
                public DBus::ObjectProxy {
   public:
    explicit Proxy(DBus::Connection *connection);
    virtual ~Proxy();

    void set_devices_changed_callback(const DevicesChangedCallback &callback);

   private:
    // Signal callbacks inherited from WiMaxManager_proxy.
    virtual void DevicesChanged(const std::vector<DBus::Path> &devices);

    // Method callbacks inherited from WiMaxManager_proxy.
    // [None]

    DevicesChangedCallback devices_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxManagerProxy);
};

}  // namespace shill

#endif  // SHILL_WIMAX_MANAGER_PROXY_H_
