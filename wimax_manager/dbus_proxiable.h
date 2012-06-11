// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DBUS_PROXIABLE_H_
#define WIMAX_MANAGER_DBUS_PROXIABLE_H_

#include <base/basictypes.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <dbus-c++/dbus.h>

#include "wimax_manager/dbus_control.h"

namespace wimax_manager {

template <typename Self, typename Proxy>
class DBusProxiable {
 public:
  DBusProxiable() {}
  ~DBusProxiable() {}

  void CreateDBusProxy() {
    if (dbus_proxy_.get())
      return;

    dbus_proxy_.reset(new(std::nothrow) Proxy(
        DBusControl::GetConnection(), static_cast<Self *>(this)));
    CHECK(dbus_proxy_.get());
  }

  void InvalidateDBusProxy() {
    dbus_proxy_.reset();
  }

  Proxy *dbus_proxy() const { return dbus_proxy_.get(); }

 private:
  scoped_ptr<Proxy> dbus_proxy_;

  DISALLOW_COPY_AND_ASSIGN(DBusProxiable);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DBUS_PROXIABLE_H_
