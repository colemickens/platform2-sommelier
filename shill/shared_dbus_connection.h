//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
