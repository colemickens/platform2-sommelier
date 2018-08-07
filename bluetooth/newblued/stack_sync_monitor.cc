// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/stack_sync_monitor.h"

#include <utility>

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>

namespace bluetooth {

StackSyncMonitor::StackSyncMonitor()
    : cached_bluez_powered_(false), weak_ptr_factory_(this) {}

void StackSyncMonitor::RegisterBluezDownCallback(dbus::Bus* bus,
                                                 base::Closure callback) {
  CHECK(callback_.is_null());
  bus->GetObjectManager(
         bluez_object_manager::kBluezObjectManagerServiceName,
         dbus::ObjectPath(bluez_object_manager::kBluezObjectManagerServicePath))
      ->RegisterInterface(bluetooth_adapter::kBluetoothAdapterInterface, this);
  callback_ = std::move(callback);
}

dbus::PropertySet* StackSyncMonitor::CreateProperties(
    dbus::ObjectProxy* object_proxy,
    const dbus::ObjectPath& object_path,
    const std::string& interface) {
  dbus::PropertySet* properties = new dbus::PropertySet(
      object_proxy, interface,
      base::Bind(&StackSyncMonitor::OnBluezPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr()));
  properties->RegisterProperty(bluetooth_adapter::kPoweredProperty,
                               &bluez_powered_);
  properties->RegisterProperty(bluetooth_adapter::kStackSyncQuittingProperty,
                               &bluez_stack_sync_quitting_);
  return properties;
}

void StackSyncMonitor::ObjectRemoved(const dbus::ObjectPath& object_path,
                                     const std::string& interface) {
  // TODO(crbug/868466): Confirm the behavior when BlueZ crashes.
  VLOG(1) << "BlueZ adapter " << object_path.value() << " removed";
}

void StackSyncMonitor::OnBluezPropertyChanged(const std::string& name) {
  VLOG(1) << "BlueZ property changed " << name;
  if (name != bluetooth_adapter::kPoweredProperty ||
      cached_bluez_powered_ == bluez_powered_.value())
    return;

  VLOG(1) << "BlueZ Powered = " << bluez_powered_.value()
          << " BlueZ StackSyncQuitting = "
          << bluez_stack_sync_quitting_.value();
  cached_bluez_powered_ = bluez_powered_.value();
  if (!cached_bluez_powered_ && !bluez_stack_sync_quitting_.value() &&
      !callback_.is_null())
    callback_.Run();
}

}  // namespace bluetooth
