// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/logging.h>
#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>

#include "shill/dbus_control.h"
#include "shill/device_dbus_adaptor.h"
#include "shill/manager_dbus_adaptor.h"
#include "shill/service_dbus_adaptor.h"

namespace shill {
DBusControl::DBusControl() {}

DBusControl::~DBusControl() {}

ManagerAdaptorInterface *DBusControl::CreateManagerAdaptor(Manager *manager) {
  EnsureDispatcher();
  DBus::Connection conn = DBus::Connection::SystemBus();
  conn.request_name(ManagerDBusAdaptor::kInterfaceName);
  return new(std::nothrow) ManagerDBusAdaptor(conn, manager);
}

ServiceAdaptorInterface *DBusControl::CreateServiceAdaptor(Service *service) {
  EnsureDispatcher();
  DBus::Connection conn = DBus::Connection::SystemBus();
  conn.request_name(ServiceDBusAdaptor::kInterfaceName);
  return new(std::nothrow) ServiceDBusAdaptor(conn, service);
}

DeviceAdaptorInterface *DBusControl::CreateDeviceAdaptor(Device *device) {
  EnsureDispatcher();
  DBus::Connection conn = DBus::Connection::SystemBus();
  conn.request_name(DeviceDBusAdaptor::kInterfaceName);
  return new(std::nothrow) DeviceDBusAdaptor(conn, device);
}

void DBusControl::EnsureDispatcher() {
  if (!dispatcher_.get()) {
    dispatcher_.reset(new(std::nothrow) DBus::Glib::BusDispatcher());
    CHECK(dispatcher_.get()) << "Failed to create a dbus-dispatcher";
    DBus::default_dispatcher = dispatcher_.get();
    dispatcher_->attach(NULL);
  }
}

}  // namespace shill
