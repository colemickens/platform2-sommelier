// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SHARED_DBUS_CONNECTION_H_
#define SHILL_SHARED_DBUS_CONNECTION_H_

#include <memory>

#include <base/lazy_instance.h>
#include <base/macros.h>

namespace DBus {
class Connection;
namespace Glib {
class BusDispatcher;
}  // namespace Glib
}  // namespace DBus

namespace shill {

class SharedDBusConnection {
 public:
  ~SharedDBusConnection() = default;

  // Since this is a singleton, use SharedDBusConnection::GetInstance()->Foo().
  static SharedDBusConnection *GetInstance();

  void Init();

  DBus::Connection *GetConnection();

 protected:
  SharedDBusConnection() = default;

 private:
  friend struct base::DefaultLazyInstanceTraits<SharedDBusConnection>;

  std::unique_ptr<DBus::Glib::BusDispatcher> dispatcher_;
  std::unique_ptr<DBus::Connection> connection_;

  DISALLOW_COPY_AND_ASSIGN(SharedDBusConnection);
};

}  // namespace shill

#endif  // SHILL_SHARED_DBUS_CONNECTION_H_
