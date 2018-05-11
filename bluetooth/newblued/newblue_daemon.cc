// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue_daemon.h"

#include <memory>
#include <string>
#include <utility>

#include <sysexits.h>

#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <chromeos/dbus/service_constants.h>

#include "bluetooth/common/dbus_daemon.h"
#include "bluetooth/newblued/libnewblue.h"

namespace {

// Chrome OS device has only 1 Bluetooth adapter, so the name is always hci0.
// TODO(sonnysasaka): Add support for more than 1 adapters.
constexpr char kAdapterObjectPath[] = "/org/bluez/hci0";

// Called when an interface of a D-Bus object is exported.
void OnInterfaceExported(std::string object_path,
                         std::string interface_name,
                         bool success) {
  VLOG(1) << "Completed interface export " << interface_name << " of object "
          << object_path << ", success = " << success;
}

}  // namespace

namespace bluetooth {

NewblueDaemon::NewblueDaemon(std::unique_ptr<Newblue> newblue)
    : newblue_(std::move(newblue)), weak_ptr_factory_(this) {}

bool NewblueDaemon::Init(scoped_refptr<dbus::Bus> bus,
                         DBusDaemon* dbus_daemon) {
  bus_ = bus;
  dbus_daemon_ = dbus_daemon;

  if (!bus_->RequestOwnershipAndBlock(
          newblue_object_manager::kNewblueObjectManagerServiceName,
          dbus::Bus::REQUIRE_PRIMARY)) {
    LOG(ERROR) << "Failed to acquire D-Bus name ownership: "
               << newblue_object_manager::kNewblueObjectManagerServiceName;
  }

  auto exported_object_manager =
      std::make_unique<brillo::dbus_utils::ExportedObjectManager>(
          bus_, dbus::ObjectPath(
                    newblue_object_manager::kNewblueObjectManagerServicePath));

  exported_object_manager_wrapper_ =
      std::make_unique<ExportedObjectManagerWrapper>(
          bus_, std::move(exported_object_manager));

  exported_object_manager_wrapper_->SetPropertyHandlerSetupCallback(
      base::Bind(&NewblueDaemon::SetupPropertyMethodHandlers,
                 weak_ptr_factory_.GetWeakPtr()));

  if (!newblue_->Init()) {
    LOG(ERROR) << "Failed initializing NewBlue";
    return false;
  }

  newblue_->ListenReadyForUp(
      base::Bind(&NewblueDaemon::OnHciReadyForUp, base::Unretained(this)));

  LOG(INFO) << "newblued initialized";
  return true;
}

void NewblueDaemon::Shutdown() {
  newblue_.reset();
  exported_object_manager_wrapper_.reset();
}

void NewblueDaemon::OnHciReadyForUp() {
  VLOG(1) << "NewBlue ready for up";

  // Workaround to avoid immediately bringing up the stack as this may result
  // in chip hang.
  // TODO(sonnysasaka): Remove this sleep when the kernel LE splitter bug
  // is fixed(https://crbug.com/852446).
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));

  if (!newblue_->BringUp()) {
    LOG(ERROR) << "error bringing up NewBlue";
    dbus_daemon_->QuitWithExitCode(EX_UNAVAILABLE);
    return;
  }
  ExportAdapterInterface();
  LOG(INFO) << "NewBlue is up";
}

void NewblueDaemon::SetupPropertyMethodHandlers(
    brillo::dbus_utils::DBusInterface* prop_interface,
    brillo::dbus_utils::ExportedPropertySet* property_set) {
  // Install standard property handlers.
  prop_interface->AddSimpleMethodHandler(
      dbus::kPropertiesGetAll, base::Unretained(property_set),
      &brillo::dbus_utils::ExportedPropertySet::HandleGetAll);
  prop_interface->AddSimpleMethodHandlerWithError(
      dbus::kPropertiesGet, base::Unretained(property_set),
      &brillo::dbus_utils::ExportedPropertySet::HandleGet);
  prop_interface->AddSimpleMethodHandlerWithError(
      dbus::kPropertiesSet, base::Unretained(property_set),
      &brillo::dbus_utils::ExportedPropertySet::HandleSet);
}

void NewblueDaemon::ExportAdapterInterface() {
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

  adapter_interface->ExportAsync(
      base::Bind(&OnInterfaceExported, adapter_object_path.value(),
                 bluetooth_adapter::kBluetoothAdapterInterface));
}

}  // namespace bluetooth
