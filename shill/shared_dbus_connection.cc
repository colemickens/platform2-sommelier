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

#include "shill/shared_dbus_connection.h"

#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>

namespace shill {

namespace {
base::LazyInstance<SharedDBusConnection> g_shared_dbus_connection =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

SharedDBusConnection* SharedDBusConnection::GetInstance() {
  return g_shared_dbus_connection.Pointer();
}

void SharedDBusConnection::Init() {
  dispatcher_.reset(new(std::nothrow) DBus::Glib::BusDispatcher());
  CHECK(dispatcher_.get()) << "Failed to create a dbus-dispatcher";
  DBus::default_dispatcher = dispatcher_.get();
  dispatcher_->attach(nullptr);
  adaptor_connection_.reset(new DBus::Connection(
      DBus::Connection::SystemBus()));
  proxy_connection_.reset(new DBus::Connection(DBus::Connection::SystemBus()));
}

DBus::Connection* SharedDBusConnection::GetAdaptorConnection() {
  CHECK(adaptor_connection_.get());
  return adaptor_connection_.get();
}

DBus::Connection* SharedDBusConnection::GetProxyConnection() {
  CHECK(proxy_connection_.get());
  return proxy_connection_.get();
}

}  // namespace shill
