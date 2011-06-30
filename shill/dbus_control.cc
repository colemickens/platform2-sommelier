// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/logging.h>
#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>

#include "shill/dbus_control.h"
#include "shill/device_dbus_adaptor.h"
#include "shill/ipconfig_dbus_adaptor.h"
#include "shill/manager_dbus_adaptor.h"
#include "shill/profile_dbus_adaptor.h"
#include "shill/service_dbus_adaptor.h"

namespace shill {
DBusControl::DBusControl() {}

DBusControl::~DBusControl() {}

DeviceAdaptorInterface *DBusControl::CreateDeviceAdaptor(Device *device) {
  connection_->request_name(DeviceDBusAdaptor::kInterfaceName);
  return new(std::nothrow) DeviceDBusAdaptor(connection_.get(), device);
}

IPConfigAdaptorInterface *DBusControl::CreateIPConfigAdaptor(IPConfig *config) {
  connection_->request_name(IPConfigDBusAdaptor::kInterfaceName);
  return new(std::nothrow) IPConfigDBusAdaptor(connection_.get(), config);
}

ManagerAdaptorInterface *DBusControl::CreateManagerAdaptor(Manager *manager) {
  connection_->request_name(ManagerDBusAdaptor::kInterfaceName);
  return new(std::nothrow) ManagerDBusAdaptor(connection_.get(), manager);
}

ProfileAdaptorInterface *DBusControl::CreateProfileAdaptor(Profile *profile) {
  connection_->request_name(ProfileDBusAdaptor::kInterfaceName);
  return new(std::nothrow) ProfileDBusAdaptor(connection_.get(), profile);
}

ServiceAdaptorInterface *DBusControl::CreateServiceAdaptor(Service *service) {
  connection_->request_name(ServiceDBusAdaptor::kInterfaceName);
  return new(std::nothrow) ServiceDBusAdaptor(connection_.get(), service);
}

void DBusControl::Init() {
  CHECK(!connection_.get());
  dispatcher_.reset(new(std::nothrow) DBus::Glib::BusDispatcher());
  CHECK(dispatcher_.get()) << "Failed to create a dbus-dispatcher";
  DBus::default_dispatcher = dispatcher_.get();
  dispatcher_->attach(NULL);
  connection_.reset(new DBus::Connection(DBus::Connection::SystemBus()));
}

}  // namespace shill
