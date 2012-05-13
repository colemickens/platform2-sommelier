// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_MANAGER_H_
#define WIMAX_MANAGER_MANAGER_H_

#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

namespace DBus {

class Connection;

}  // namespace DBus

namespace wimax_manager {

class Device;
class Driver;
class ManagerDBusAdaptor;

class Manager {
 public:
  explicit Manager(DBus::Connection *dbus_connection);
  virtual ~Manager();

  bool Initialize();
  bool Finalize();

  const std::vector<Device *> &devices() const { return devices_; }

 private:
  DBus::Connection *dbus_connection_;
  scoped_ptr<Driver> driver_;
  std::vector<Device *> devices_;
  scoped_ptr<ManagerDBusAdaptor> adaptor_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_MANAGER_H_
