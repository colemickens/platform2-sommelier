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
  EnsureConnection();
  connection_->request_name(ManagerDBusAdaptor::kInterfaceName);
  return new(std::nothrow) ManagerDBusAdaptor(connection_.get(), manager);
}

ServiceAdaptorInterface *DBusControl::CreateServiceAdaptor(Service *service) {
  EnsureConnection();
  connection_->request_name(ServiceDBusAdaptor::kInterfaceName);
  return new(std::nothrow) ServiceDBusAdaptor(connection_.get(), service);
}

DeviceAdaptorInterface *DBusControl::CreateDeviceAdaptor(Device *device) {
  EnsureConnection();
  connection_->request_name(DeviceDBusAdaptor::kInterfaceName);
  return new(std::nothrow) DeviceDBusAdaptor(connection_.get(), device);
}

void DBusControl::EnsureConnection() {
  if (!connection_.get()) {
    dispatcher_.reset(new(std::nothrow) DBus::Glib::BusDispatcher());
    CHECK(dispatcher_.get()) << "Failed to create a dbus-dispatcher";
    DBus::default_dispatcher = dispatcher_.get();
    dispatcher_->attach(NULL);
    connection_.reset(new DBus::Connection(DBus::Connection::SystemBus()));
  }
}

}  // namespace shill
