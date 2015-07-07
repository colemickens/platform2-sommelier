// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_dbus_control.h"

#include "shill/dbus/chromeos_device_dbus_adaptor.h"
#include "shill/dbus/chromeos_ipconfig_dbus_adaptor.h"
#include "shill/dbus/chromeos_manager_dbus_adaptor.h"
#include "shill/dbus/chromeos_profile_dbus_adaptor.h"
#include "shill/dbus/chromeos_rpc_task_dbus_adaptor.h"
#include "shill/dbus/chromeos_service_dbus_adaptor.h"
#include "shill/dbus/chromeos_third_party_vpn_dbus_adaptor.h"

using chromeos::dbus_utils::ExportedObjectManager;

namespace shill {

ChromeosDBusControl::ChromeosDBusControl(
    const base::WeakPtr<ExportedObjectManager>& object_manager,
    const scoped_refptr<dbus::Bus>& bus)
    : object_manager_(object_manager),
      bus_(bus) {}

ChromeosDBusControl::~ChromeosDBusControl() {}

template <typename Object, typename AdaptorInterface, typename Adaptor>
AdaptorInterface* ChromeosDBusControl::CreateAdaptor(Object* object) {
  return new Adaptor(object_manager_, bus_, object);
}

DeviceAdaptorInterface* ChromeosDBusControl::CreateDeviceAdaptor(
    Device* device) {
  return
      CreateAdaptor<Device, DeviceAdaptorInterface, ChromeosDeviceDBusAdaptor>(
          device);
}

IPConfigAdaptorInterface* ChromeosDBusControl::CreateIPConfigAdaptor(
    IPConfig* config) {
  return
      CreateAdaptor<IPConfig, IPConfigAdaptorInterface,
                    ChromeosIPConfigDBusAdaptor>(config);
}

ManagerAdaptorInterface* ChromeosDBusControl::CreateManagerAdaptor(
    Manager* manager) {
  return
      CreateAdaptor<Manager, ManagerAdaptorInterface,
                    ChromeosManagerDBusAdaptor>(manager);
}

ProfileAdaptorInterface* ChromeosDBusControl::CreateProfileAdaptor(
    Profile* profile) {
  return
      CreateAdaptor<Profile, ProfileAdaptorInterface,
                    ChromeosProfileDBusAdaptor>(profile);
}

RPCTaskAdaptorInterface* ChromeosDBusControl::CreateRPCTaskAdaptor(
    RPCTask* task) {
  return
      CreateAdaptor<RPCTask, RPCTaskAdaptorInterface,
                    ChromeosRPCTaskDBusAdaptor>(task);
}

ServiceAdaptorInterface* ChromeosDBusControl::CreateServiceAdaptor(
    Service* service) {
  return
      CreateAdaptor<Service, ServiceAdaptorInterface,
                    ChromeosServiceDBusAdaptor>(service);
}

#ifndef DISABLE_VPN
ThirdPartyVpnAdaptorInterface* ChromeosDBusControl::CreateThirdPartyVpnAdaptor(
    ThirdPartyVpnDriver* driver) {
  return
      CreateAdaptor<ThirdPartyVpnDriver, ThirdPartyVpnAdaptorInterface,
                    ChromeosThirdPartyVpnDBusAdaptor>(driver);
}
#endif

}  // namespace shill
