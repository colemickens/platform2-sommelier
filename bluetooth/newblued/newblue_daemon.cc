// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue_daemon.h"

#include <memory>
#include <utility>

#include <sysexits.h>

#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <chromeos/dbus/service_constants.h>

#include "bluetooth/common/dbus_daemon.h"

namespace bluetooth {

NewblueDaemon::NewblueDaemon(std::unique_ptr<Newblue> newblue,
                             bool is_idle_mode)
    : is_idle_mode_(is_idle_mode),
      newblue_(std::move(newblue)),
      weak_ptr_factory_(this) {}

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

  if (is_idle_mode_) {
    LOG(INFO) << "LE splitter not enabled, newblued running in idle mode.";
    LOG(INFO) << "To enable, run 'newblue enable' in crosh and reboot.";
    return true;
  }

  auto exported_object_manager =
      std::make_unique<brillo::dbus_utils::ExportedObjectManager>(
          bus_, dbus::ObjectPath(
                    newblue_object_manager::kNewblueObjectManagerServicePath));

  exported_object_manager_wrapper_ =
      std::make_unique<ExportedObjectManagerWrapper>(
          bus_, std::move(exported_object_manager));

  if (!newblue_->Init()) {
    LOG(ERROR) << "Failed initializing NewBlue";
    return false;
  }

  adapter_interface_handler_ = std::make_unique<AdapterInterfaceHandler>(
      bus_, newblue_.get(), exported_object_manager_wrapper_.get());
  device_interface_handler_ = std::make_unique<DeviceInterfaceHandler>(
      bus_, newblue_.get(), exported_object_manager_wrapper_.get());
  advertising_manager_interface_handler_ =
      std::make_unique<AdvertisingManagerInterfaceHandler>(
          newblue_->libnewblue(), bus_, exported_object_manager_wrapper_.get());
  agent_manager_interface_handler_ =
      std::make_unique<AgentManagerInterfaceHandler>(
          bus_, exported_object_manager_wrapper_.get());
  debug_manager_ = std::make_unique<DebugManager>(bus_);

  debug_manager_->Init();
  agent_manager_interface_handler_->Init();
  newblue_->RegisterPairingAgent(agent_manager_interface_handler_.get());

  gatt_ =
      std::make_unique<Gatt>(newblue_.get(), device_interface_handler_.get());

  if (!newblue_->ListenReadyForUp(base::Bind(&NewblueDaemon::OnHciReadyForUp,
                                             base::Unretained(this)))) {
    LOG(ERROR) << "Error listening to HCI ready for up";
    return false;
  }

  LOG(INFO) << "newblued initialized";
  return true;
}

void NewblueDaemon::Shutdown() {
  newblue_->UnregisterPairingAgent();

  gatt_.reset();

  newblue_.reset();
  agent_manager_interface_handler_.reset();
  device_interface_handler_.reset();
  exported_object_manager_wrapper_.reset();
  debug_manager_.reset();
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
  adapter_interface_handler_->Init(device_interface_handler_.get());
  stack_sync_monitor_.RegisterBluezDownCallback(
      bus_.get(),
      base::Bind(&NewblueDaemon::OnBluezDown, weak_ptr_factory_.GetWeakPtr()));
  LOG(INFO) << "NewBlue is up";

  advertising_manager_interface_handler_->Init();

  if (!device_interface_handler_->Init()) {
    LOG(ERROR) << "Failed to initialize device interface";
    dbus_daemon_->QuitWithExitCode(EX_UNAVAILABLE);
    return;
  }
}

void NewblueDaemon::OnBluezDown() {
  ExportedInterface* adapter_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          dbus::ObjectPath(kAdapterObjectPath),
          bluetooth_adapter::kBluetoothAdapterInterface);
  if (!adapter_interface)
    return;

  LOG(INFO) << "Quitting due to BlueZ down detected";
  adapter_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_adapter::kStackSyncQuittingProperty)
      ->SetValue(true);
  exit(0);  // TODO(crbug/873905): Quit gracefully after this is fixed.
}

}  // namespace bluetooth
