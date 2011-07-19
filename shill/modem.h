// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_
#define SHILL_MODEM_

#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

namespace shill {

class ControlInterface;
class DBusPropertiesProxyInterface;
class EventDispatcher;
class Manager;

// Handles an instance of ModemManager.Modem and an instance of CellularDevice.
class Modem {
 public:
  Modem(const std::string &owner,
        const std::string &path,
        ControlInterface *control_interface,
        EventDispatcher *dispatcher,
        Manager *manager);
  ~Modem();

 private:
  scoped_ptr<DBusPropertiesProxyInterface> dbus_properties_proxy_;

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Manager *manager_;

  DISALLOW_COPY_AND_ASSIGN(Modem);
};

}  // namespace shill

#endif  // SHILL_MODEM_
