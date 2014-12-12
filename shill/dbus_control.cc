// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_control.h"

#include <string>

#include <dbus-c++/dbus.h>

#include "shill/device_dbus_adaptor.h"
#include "shill/ipconfig_dbus_adaptor.h"
#include "shill/logging.h"
#include "shill/manager_dbus_adaptor.h"
#include "shill/profile_dbus_adaptor.h"
#include "shill/rpc_task_dbus_adaptor.h"
#include "shill/service_dbus_adaptor.h"
#include "shill/shared_dbus_connection.h"
#include "shill/vpn/third_party_vpn_dbus_adaptor.h"

namespace shill {

DBusControl::DBusControl() {}

DBusControl::~DBusControl() {}

template <typename Object, typename AdaptorInterface, typename Adaptor>
AdaptorInterface *DBusControl::CreateAdaptor(Object *object) {
  AdaptorInterface *adaptor = nullptr;
  try {
    adaptor = new Adaptor(GetConnection(), object);
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

#ifndef DISABLE_VPN
ThirdPartyVpnAdaptorInterface *DBusControl::CreateThirdPartyVpnAdaptor(
    ThirdPartyVpnDriver *driver) {
  return CreateAdaptor<ThirdPartyVpnDriver, ThirdPartyVpnAdaptorInterface,
                       ThirdPartyVpnAdaptor>(driver);
}
#endif

void DBusControl::Init() {
  if (!GetConnection()->acquire_name(SHILL_INTERFACE)) {
    LOG(FATAL) << "Failed to acquire D-Bus name " << SHILL_INTERFACE << ". "
               << "Is another shill running?";
  }
}

DBus::Connection *DBusControl::GetConnection() const {
  return SharedDBusConnection::GetInstance()->GetConnection();
}

}  // namespace shill
