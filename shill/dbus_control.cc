// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_control.h"

#include <string>

#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>

#include "shill/device_dbus_adaptor.h"
#include "shill/ipconfig_dbus_adaptor.h"
#include "shill/logging.h"
#include "shill/manager_dbus_adaptor.h"
#include "shill/profile_dbus_adaptor.h"
#include "shill/rpc_task_dbus_adaptor.h"
#include "shill/service_dbus_adaptor.h"

namespace shill {

DBusControl::DBusControl() {}

DBusControl::~DBusControl() {}

DeviceAdaptorInterface *DBusControl::CreateDeviceAdaptor(Device *device) {
  DeviceAdaptorInterface *result = NULL;
  try {
    result = new DeviceDBusAdaptor(connection_.get(), device);
  } catch(const DBus::Error &error) {
    LOG(ERROR) << error.message();
  }
  return result;
}

IPConfigAdaptorInterface *DBusControl::CreateIPConfigAdaptor(IPConfig *config) {
  IPConfigAdaptorInterface *result = NULL;
  try {
    result = new IPConfigDBusAdaptor(connection_.get(), config);
  } catch(const DBus::Error &error) {
    LOG(ERROR) << error.message();
  }
  return result;
}

ManagerAdaptorInterface *DBusControl::CreateManagerAdaptor(Manager *manager) {
  ManagerAdaptorInterface *result = NULL;
  try {
    result = new ManagerDBusAdaptor(connection_.get(), manager);
  } catch(const DBus::Error &error) {
    LOG(ERROR) << error.message();
  }
  return result;
}

ProfileAdaptorInterface *DBusControl::CreateProfileAdaptor(Profile *profile) {
  ProfileAdaptorInterface *result = NULL;
  try {
    result = new ProfileDBusAdaptor(connection_.get(), profile);
  } catch(const DBus::Error &error) {
    LOG(ERROR) << error.message();
  }
  return result;
}

RPCTaskAdaptorInterface *DBusControl::CreateRPCTaskAdaptor(RPCTask *task) {
  RPCTaskAdaptorInterface *result = NULL;
  try {
    result = new RPCTaskDBusAdaptor(connection_.get(), task);
  } catch(const DBus::Error &error) {
    LOG(ERROR) << error.message();
  }
  return result;
}

ServiceAdaptorInterface *DBusControl::CreateServiceAdaptor(Service *service) {
  ServiceAdaptorInterface *result = NULL;
  try {
    result = new ServiceDBusAdaptor(connection_.get(), service);
  } catch(const DBus::Error &error) {
    LOG(ERROR) << error.message();
  }
  return result;
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
