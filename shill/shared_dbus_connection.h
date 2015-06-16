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
  static SharedDBusConnection* GetInstance();

  void Init();

  // Returns a DBus connection that may be attached to a name instance.
  // This is useful for adaptor instances which handle incoming method calls.
  DBus::Connection* GetAdaptorConnection();

  // Returns a DBus connection that is not associated with an acquired name.
  // This is useful for proxy instances which handle incoming signals and
  // outgoing method calls.
  DBus::Connection* GetProxyConnection();

 protected:
  SharedDBusConnection() = default;

 private:
  friend struct base::DefaultLazyInstanceTraits<SharedDBusConnection>;

  std::unique_ptr<DBus::Glib::BusDispatcher> dispatcher_;
  std::unique_ptr<DBus::Connection> adaptor_connection_;
  std::unique_ptr<DBus::Connection> proxy_connection_;

  DISALLOW_COPY_AND_ASSIGN(SharedDBusConnection);
};

}  // namespace shill

#endif  // SHILL_SHARED_DBUS_CONNECTION_H_
