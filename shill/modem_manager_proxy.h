// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_MANAGER_PROXY_
#define SHILL_MODEM_MANAGER_PROXY_

#include <base/basictypes.h>

#include "shill/mm.h"
#include "shill/modem_manager_proxy_interface.h"

namespace shill {

class ModemManager;

// There's a single proxy per ModemManager service identified by its DBus |path|
// and owner name |service|.
class ModemManagerProxy : public ModemManagerProxyInterface {
 public:
  ModemManagerProxy(ModemManager *manager,
                    const std::string &path,
                    const std::string &service);
  virtual ~ModemManagerProxy();

  // Inherited from ModemManagerProxyInterface.
  virtual std::vector<DBus::Path> EnumerateDevices();

 private:
  class Proxy : public org::freedesktop::ModemManager_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(ModemManager *manager,
          DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from ModemManager_proxy.
    virtual void DeviceAdded(const DBus::Path &device);
    virtual void DeviceRemoved(const DBus::Path &device);

    // The owner of this proxy.
    ModemManager *manager_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  DBus::Connection connection_;
  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemManagerProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_MANAGER_PROXY_
