// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DBUS_SERVICE_H_
#define WIMAX_MANAGER_DBUS_SERVICE_H_

#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

#include "wimax_manager/dbus_proxiable.h"

namespace wimax_manager {

class DBusServiceDBusProxy;
class Manager;
class PowerManager;

class DBusService : public DBusProxiable<DBusService, DBusServiceDBusProxy> {
 public:
  explicit DBusService(Manager *manager);
  ~DBusService();

  void Initialize();
  void Finalize();
  bool NameHasOwner(const std::string &name);
  void OnNameOwnerChanged(const std::string &name,
                          const std::string &old_owner,
                          const std::string &new_owner);

 private:
  void SetPowerManager(PowerManager *power_manager);

  Manager *manager_;
  scoped_ptr<PowerManager> power_manager_;

  DISALLOW_COPY_AND_ASSIGN(DBusService);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DBUS_SERVICE_H_
