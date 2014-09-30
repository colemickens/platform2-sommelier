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

template <typename Object, typename AdaptorInterface, typename Adaptor>
AdaptorInterface *DBusControl::CreateAdaptor(Object *object) {
  AdaptorInterface *adaptor = nullptr;
  try {
    adaptor = new Adaptor(connection_.get(), object);
  } catch(const DBus::ErrorObjectPathInUse &error) {
    LOG(FATAL) << error.message() << " (object path in use)";
  } catch(const DBus::ErrorNoMemory &error) {
    LOG(FATAL) << error.message() << " (no memory)";
  } catch(const DBus::Error &error) {
    LOG(FATAL) << error.message();
  }
  return adaptor;
}

DeviceAdaptorInterface *DBusControl::CreateDeviceAdaptor(Device *device) {
  return CreateAdaptor<Device, DeviceAdaptorInterface, DeviceDBusAdaptor>(
      device);
}

IPConfigAdaptorInterface *DBusControl::CreateIPConfigAdaptor(IPConfig *config) {
  return CreateAdaptor<IPConfig, IPConfigAdaptorInterface, IPConfigDBusAdaptor>(
      config);
}

ManagerAdaptorInterface *DBusControl::CreateManagerAdaptor(Manager *manager) {
  return CreateAdaptor<Manager, ManagerAdaptorInterface, ManagerDBusAdaptor>(
      manager);
}

ProfileAdaptorInterface *DBusControl::CreateProfileAdaptor(Profile *profile) {
  return CreateAdaptor<Profile, ProfileAdaptorInterface, ProfileDBusAdaptor>(
      profile);
}

RPCTaskAdaptorInterface *DBusControl::CreateRPCTaskAdaptor(RPCTask *task) {
  return CreateAdaptor<RPCTask, RPCTaskAdaptorInterface, RPCTaskDBusAdaptor>(
      task);
}

ServiceAdaptorInterface *DBusControl::CreateServiceAdaptor(Service *service) {
  return CreateAdaptor<Service, ServiceAdaptorInterface, ServiceDBusAdaptor>(
      service);
}

void DBusControl::Init() {
  CHECK(!connection_.get());
  dispatcher_.reset(new(std::nothrow) DBus::Glib::BusDispatcher());
  CHECK(dispatcher_.get()) << "Failed to create a dbus-dispatcher";
  DBus::default_dispatcher = dispatcher_.get();
  dispatcher_->attach(nullptr);
  connection_.reset(new DBus::Connection(DBus::Connection::SystemBus()));
  if (!connection_->acquire_name(SHILL_INTERFACE)) {
    LOG(FATAL) << "Failed to acquire D-Bus name " << SHILL_INTERFACE << ". "
               << "Is another shill running?";
  }
}

}  // namespace shill
