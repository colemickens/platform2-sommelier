// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_dbus_control.h"

#include "shill/dbus/chromeos_device_dbus_adaptor.h"

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
  return nullptr;
}

ManagerAdaptorInterface* ChromeosDBusControl::CreateManagerAdaptor(
    Manager* manager) {
  return nullptr;
}

ProfileAdaptorInterface* ChromeosDBusControl::CreateProfileAdaptor(
    Profile* profile) {
  return nullptr;
}

RPCTaskAdaptorInterface* ChromeosDBusControl::CreateRPCTaskAdaptor(
    RPCTask* task) {
  return nullptr;
}

ServiceAdaptorInterface* ChromeosDBusControl::CreateServiceAdaptor(
    Service* service) {
  return nullptr;
}

#ifndef DISABLE_VPN
ThirdPartyVpnAdaptorInterface* ChromeosDBusControl::CreateThirdPartyVpnAdaptor(
    ThirdPartyVpnDriver* driver) {
  return nullptr;
}
#endif

}  // namespace shill
