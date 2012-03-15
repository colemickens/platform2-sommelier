// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_control.h"

#include <string>

#include <base/logging.h>
#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>

#include "shill/device_dbus_adaptor.h"
#include "shill/ipconfig_dbus_adaptor.h"
#include "shill/manager_dbus_adaptor.h"
#include "shill/profile_dbus_adaptor.h"
#include "shill/rpc_task_dbus_adaptor.h"
#include "shill/service_dbus_adaptor.h"

namespace shill {

DBusControl::DBusControl() {}

DBusControl::~DBusControl() {}

DeviceAdaptorInterface *DBusControl::CreateDeviceAdaptor(Device *device) {
  return new(std::nothrow) DeviceDBusAdaptor(connection_.get(), device);
}

IPConfigAdaptorInterface *DBusControl::CreateIPConfigAdaptor(IPConfig *config) {
  return new(std::nothrow) IPConfigDBusAdaptor(connection_.get(), config);
}

ManagerAdaptorInterface *DBusControl::CreateManagerAdaptor(Manager *manager) {
  return new(std::nothrow) ManagerDBusAdaptor(connection_.get(), manager);
}

ProfileAdaptorInterface *DBusControl::CreateProfileAdaptor(Profile *profile) {
  return new(std::nothrow) ProfileDBusAdaptor(connection_.get(), profile);
}

RPCTaskAdaptorInterface *DBusControl::CreateRPCTaskAdaptor(RPCTask *task) {
  return new(std::nothrow) RPCTaskDBusAdaptor(connection_.get(), task);
}

ServiceAdaptorInterface *DBusControl::CreateServiceAdaptor(Service *service) {
  return new(std::nothrow) ServiceDBusAdaptor(connection_.get(), service);
}

void DBusControl::Init() {
  CHECK(!connection_.get());
  dispatcher_.reset(new(std::nothrow) DBus::Glib::BusDispatcher());
  CHECK(dispatcher_.get()) << "Failed to create a dbus-dispatcher";
  DBus::default_dispatcher = dispatcher_.get();
  dispatcher_->attach(NULL);
  connection_.reset(new DBus::Connection(DBus::Connection::SystemBus()));
  connection_->request_name(SHILL_INTERFACE);
}

}  // namespace shill
