// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/device.h"

#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <shill/net/ieee80211.h>

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;
using shill::ByteString;
using std::string;

namespace apmanager {

Device::Device(const string& device_name)
    : org::chromium::apmanager::DeviceAdaptor(this) {
  SetDeviceName(device_name);
  SetInUsed(false);
}

Device::~Device() {}

void Device::RegisterAsync(ExportedObjectManager* object_manager,
                           AsyncEventSequencer* sequencer,
                           int device_identifier) {
  CHECK(!dbus_object_) << "Already registered";
  dbus_path_ = dbus::ObjectPath(
      base::StringPrintf("%s/devices/%d",
                         kManagerPath,
                         device_identifier));
  dbus_object_.reset(
      new chromeos::dbus_utils::DBusObject(
          object_manager,
          object_manager ? object_manager->GetBus() : nullptr,
          dbus_path_));
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Config.RegisterAsync() failed.", true));
}

void Device::RegisterInterface(const WiFiInterface& new_interface) {
  LOG(INFO) << "RegisteringInterface " << new_interface.iface_name
            << " on device " << GetDeviceName();
  for (const auto& interface : interface_list_) {
    // Done if interface already in the list.
    if (interface.iface_index == new_interface.iface_index) {
      LOG(INFO) << "Interface " << new_interface.iface_name
                << " already registered.";
      return;
    }
  }
  interface_list_.push_back(new_interface);
  UpdatePreferredAPInterface();
}

void Device::DeregisterInterface(const WiFiInterface& interface) {
  LOG(INFO) << "DeregisteringInterface " << interface.iface_name
            << " on device " << GetDeviceName();
  for (auto it = interface_list_.begin(); it != interface_list_.end(); ++it) {
    if (it->iface_index == interface.iface_index) {
      interface_list_.erase(it);
      UpdatePreferredAPInterface();
      return;
    }
  }
}

void Device::ParseWiphyCapability(const shill::Nl80211Message& msg) {
  // TODO(zqiu): Parse capabilities.
}

bool Device::ClaimDevice() {
  if (GetInUsed()) {
    LOG(ERROR) << "Failed to claim device [" << GetDeviceName()
               << "]: already in used.";
    return false;
  }

  // TODO(zqiu): Issue dbus call to shill to claim all interfaces on this
  // device.

  SetInUsed(true);
  return true;
}

bool Device::ReleaseDevice() {
  if (!GetInUsed()) {
    LOG(ERROR) << "Failed to release device [" << GetDeviceName()
               << "]: not currently in-used.";
    return false;
  }

  // TODO(zqiu): Issue dbus call to shill to release all interfaces on this
  // device.

  SetInUsed(false);
  return true;
}

bool Device::InterfaceExists(const string& interface_name) {
  for (const auto& interface : interface_list_) {
    if (interface.iface_name == interface_name) {
      return true;
    }
  }
  return false;
}

void Device::UpdatePreferredAPInterface() {
  // Use the first registered AP mode interface if there is one, otherwise use
  // the first registered managed mode interface. If none are available, then
  // no interface can be used for AP operation on this device.
  WiFiInterface preferred_interface;
  for (const auto& interface : interface_list_) {
    if (interface.iface_type == NL80211_IFTYPE_AP) {
      preferred_interface = interface;
      break;
    } else if (interface.iface_type == NL80211_IFTYPE_STATION &&
               preferred_interface.iface_name.empty()) {
      preferred_interface = interface;
    }
    // Ignore all other interface types.
  }
  // Update preferred AP interface property.
  SetPreferredApInterface(preferred_interface.iface_name);
}

}  // namespace apmanager
