// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DBUS_CONTROL_H_
#define WIMAX_MANAGER_DBUS_CONTROL_H_

#include <base/basictypes.h>
#include <base/lazy_instance.h>
#include <base/memory/scoped_ptr.h>

namespace DBus {

class Connection;

namespace Glib {

class BusDispatcher;

}  // namespace Glib

}  // namespace DBus

namespace wimax_manager {

class DBusControl {
 public:
  static DBus::Connection *GetConnection();

 private:
  friend struct base::DefaultLazyInstanceTraits<DBusControl>;

  DBusControl();
  ~DBusControl();

  static DBusControl *GetInstance();

  void Initialize();
  void Finalize();

  scoped_ptr<DBus::Glib::BusDispatcher> bus_dispatcher_;
  scoped_ptr<DBus::Connection> connection_;

  DISALLOW_COPY_AND_ASSIGN(DBusControl);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DBUS_CONTROL_H_
