// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_MANAGER_H_
#define WIMAX_MANAGER_MANAGER_H_

#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>

#include "wimax_manager/dbus_adaptable.h"

namespace wimax_manager {

class Device;
class Driver;
class ManagerDBusAdaptor;

class Manager : public DBusAdaptable<Manager, ManagerDBusAdaptor> {
 public:
  Manager();
  virtual ~Manager();

  bool Initialize();
  bool Finalize();

  const std::vector<Device *> &devices() const { return devices_.get(); }

 private:
  scoped_ptr<Driver> driver_;
  ScopedVector<Device> devices_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_MANAGER_H_
