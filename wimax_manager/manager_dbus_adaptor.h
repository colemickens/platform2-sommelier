// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_MANAGER_DBUS_ADAPTOR_H_
#define WIMAX_MANAGER_MANAGER_DBUS_ADAPTOR_H_

#include "wimax_manager/dbus_adaptor.h"
#include "wimax_manager/dbus_bindings/manager.h"

namespace wimax_manager {

class Manager;

class ManagerDBusAdaptor : public org::chromium::WiMaxManager_adaptor,
                           public DBusAdaptor {
 public:
  ManagerDBusAdaptor(DBus::Connection *connection, Manager *manager);
  virtual ~ManagerDBusAdaptor();

 private:
  Manager *manager_;

  DISALLOW_COPY_AND_ASSIGN(ManagerDBusAdaptor);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_MANAGER_DBUS_ADAPTOR_H_
