// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/adapter_interface_handler.h"

#include <brillo/errors/error.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/object_path.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/common/util.h"
#include "bluetooth/newblued/newblue.h"

namespace bluetooth {

AdapterInterfaceHandler::AdapterInterfaceHandler(
    scoped_refptr<dbus::Bus> bus,
    Newblue* newblue,
    ExportedObjectManagerWrapper* exported_object_manager_wrapper)
    : bus_(bus),
      newblue_(newblue),
      exported_object_manager_wrapper_(exported_object_manager_wrapper),
      weak_ptr_factory_(this) {}

void AdapterInterfaceHandler::Init(
    Newblue::DeviceDiscoveredCallback device_discovered_callback) {
  device_discovered_callback_ = device_discovered_callback;
  dbus::ObjectPath adapter_object_path(kAdapterObjectPath);
  exported_object_manager_wrapper_->AddExportedInterface(
      adapter_object_path, bluetooth_adapter::kBluetoothAdapterInterface);
  ExportedInterface* adapter_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          adapter_object_path, bluetooth_adapter::kBluetoothAdapterInterface);

  // Expose the "Powered" property of the adapter. This property is only
  // controlled by BlueZ, so newblued's "Powered" property is ignored by
  // btdispatch. However, it is useful to have the dummy "Powered" property
  // for testing when Chrome (or any client) connects directly to newblued
  // instead of via btdispatch.
  adapter_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_adapter::kPoweredProperty)
      ->SetValue(true);
  adapter_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_adapter::kStackSyncQuittingProperty)
      ->SetValue(false);

  adapter_interface->AddSimpleMethodHandlerWithErrorAndMessage(
      bluetooth_adapter::kStartDiscovery, base::Unretained(this),
      &AdapterInterfaceHandler::HandleStartDiscovery);
  adapter_interface->AddSimpleMethodHandlerWithErrorAndMessage(
      bluetooth_adapter::kStopDiscovery, base::Unretained(this),
      &AdapterInterfaceHandler::HandleStopDiscovery);

  adapter_interface->ExportAsync(
      base::Bind(&OnInterfaceExported, adapter_object_path.value(),
                 bluetooth_adapter::kBluetoothAdapterInterface));
}

bool AdapterInterfaceHandler::HandleStartDiscovery(brillo::ErrorPtr* error,
                                                   dbus::Message* message) {
  VLOG(1) << __func__;
  bool ret = newblue_->StartDiscovery(device_discovered_callback_);
  if (!ret) {
    brillo::Error::AddTo(error, FROM_HERE, brillo::errors::dbus::kDomain,
                         bluetooth_adapter::kErrorFailed,
                         "Failed to start discovery");
  }
  return ret;
}

bool AdapterInterfaceHandler::HandleStopDiscovery(brillo::ErrorPtr* error,
                                                  dbus::Message* message) {
  VLOG(1) << __func__;
  bool ret = newblue_->StopDiscovery();
  if (!ret) {
    brillo::Error::AddTo(error, FROM_HERE, brillo::errors::dbus::kDomain,
                         bluetooth_adapter::kErrorFailed,
                         "Failed to stop discovery");
  }
  return ret;
}

}  // namespace bluetooth
