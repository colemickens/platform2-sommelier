// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DAEMON_H_
#define WIMAX_MANAGER_DAEMON_H_

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

namespace DBus {

class Connection;

namespace Glib {

class BusDispatcher;

}  // namespace Glib

}  // namespace DBus

namespace wimax_manager {

class Manager;

class Daemon {
 public:
  Daemon();
  ~Daemon();

  bool Initialize();
  bool Finalize();

 private:
  scoped_ptr<DBus::Glib::BusDispatcher> dbus_dispatcher_;
  scoped_ptr<DBus::Connection> dbus_connection_;
  scoped_ptr<Manager> manager_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DAEMON_H_
