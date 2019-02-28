// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/suspend_manager.h"

#include <memory>
#include <string>

#include <base/bind.h>
#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/object_proxy.h>

#include "power_manager/proto_bindings/suspend.pb.h"

namespace bluetooth {

namespace {

// Description for power manager's RegisterSuspendDelay.
constexpr char kSuspendDelayDescription[] = "btdispatch";

// Timeout for power manager's SuspendImminent wait.
// Bluez's PauseDiscovery should take less than 5 seconds to complete.
constexpr base::TimeDelta kSuspendDelayTimeout =
    base::TimeDelta::FromSeconds(5);

// Used for ObjectProxy::ConnectToSignal callbacks.
void HandleSignalConnected(const std::string& interface,
                           const std::string& signal,
                           bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to connect to signal " << signal << " of interface "
               << interface;
  }
}

}  // namespace

// Though BlueZ doesn't hardcode "hci0" as a constant, Chrome OS devices only
// use one Bluetooth adapter per device so the "hci0" is always constant.
const char SuspendManager::kBluetoothAdapterObjectPath[] = "/org/bluez/hci0";

SuspendManager::SuspendManager(scoped_refptr<dbus::Bus> bus)
    : bus_(bus), weak_ptr_factory_(this) {}

SuspendManager::~SuspendManager() = default;

void SuspendManager::Init() {
  // Initialize D-Bus proxies.
  power_manager_dbus_proxy_ = bus_->GetObjectProxy(
      power_manager::kPowerManagerServiceName,
      dbus::ObjectPath(power_manager::kPowerManagerServicePath));
  btdispatch_dbus_proxy_ = bus_->GetObjectProxy(
      bluetooth_object_manager::kBluetoothObjectManagerServiceName,
      dbus::ObjectPath(kBluetoothAdapterObjectPath));

  service_watcher_ =
      std::make_unique<ServiceWatcher>(power_manager_dbus_proxy_);
  service_watcher_->RegisterWatcher(
      base::Bind(&SuspendManager::HandlePowerManagerAvailableOrRestarted,
                 weak_ptr_factory_.GetWeakPtr()));

  // Prepare power manager event handlers.
  power_manager_dbus_proxy_->ConnectToSignal(
      power_manager::kPowerManagerInterface,
      power_manager::kSuspendImminentSignal,
      base::Bind(&SuspendManager::HandleSuspendImminentSignal,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&HandleSignalConnected));
  power_manager_dbus_proxy_->ConnectToSignal(
      power_manager::kPowerManagerInterface, power_manager::kSuspendDoneSignal,
      base::Bind(&SuspendManager::HandleSuspendDoneSignal,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&HandleSignalConnected));
}

void SuspendManager::HandlePowerManagerAvailableOrRestarted(bool available) {
  if (!available) {
    LOG(INFO) << "Power manager becomes not available";
    // Power manager is dead, undo suspend to make sure we're not stack in
    // suspend mode forever, and reset delay id to 0 to mark that we aren't
    // currently registered
    suspend_delay_id_ = 0;
    HandleSuspendDone();
    return;
  }

  power_manager::RegisterSuspendDelayRequest request;
  request.set_timeout(kSuspendDelayTimeout.ToInternalValue());
  request.set_description(kSuspendDelayDescription);
  dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                               power_manager::kRegisterSuspendDelayMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);

  VLOG(1) << "Calling RegisterSuspendDelay to powerd";
  power_manager_dbus_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&SuspendManager::OnSuspendDelayRegistered,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SuspendManager::HandleSuspendImminentSignal(dbus::Signal* signal) {
  VLOG(1) << "Received SuspendImminent signal from powerd";

  // Do nothing if we haven't registered a suspend delay with power manager.
  if (!suspend_delay_id_)
    return;

  dbus::MessageReader reader(signal);
  power_manager::SuspendImminent suspend_imminent;
  if (!reader.PopArrayOfBytesAsProto(&suspend_imminent)) {
    LOG(ERROR) << "Unable to parse SuspendImminent signal";
    return;
  }
  HandleSuspendImminent(suspend_imminent.suspend_id());
}

void SuspendManager::HandleSuspendDoneSignal(dbus::Signal* signal) {
  VLOG(1) << "Received SuspendDone signal from powerd";

  // Do nothing if we haven't registered a suspend delay with power manager.
  if (!suspend_delay_id_)
    return;

  HandleSuspendDone();
}

void SuspendManager::OnSuspendDelayRegistered(dbus::Response* response) {
  VLOG(1) << "Received return of RegisterSuspendDelay from powerd";

  // RegisterSuspendDelay has returned from power manager, keeps the delay id.
  power_manager::RegisterSuspendDelayReply reply;
  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfBytesAsProto(&reply)) {
    LOG(ERROR) << "Unable to parse RegisterSuspendDelayReply";
    return;
  }
  suspend_delay_id_ = reply.delay_id();
}

void SuspendManager::OnSuspendImminentHandled(dbus::Response* response) {
  VLOG(1) << "Received return of SuspendImminent from BlueZ and NewBlue";

  is_suspend_operation_in_progress_ = false;

  if (!suspend_id_) {
    // Looks like SuspendDone arrived before our suspend preparation finished,
    // so here we undo our suspend preparation.
    HandleSuspendDone();
    return;
  }

  // Bluez and NewBlue SuspendImminent has finished, lets power manager know
  // that we are ready to suspend.
  power_manager::SuspendReadinessInfo suspend_readiness;
  suspend_readiness.set_suspend_id(suspend_id_);
  suspend_readiness.set_delay_id(suspend_delay_id_);

  // Suspend preparation is done, so reset this to 0.
  suspend_id_ = 0;

  dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                               power_manager::kHandleSuspendReadinessMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(suspend_readiness);

  VLOG(1) << "Calling HandleSuspendReadiness to powerd";
  power_manager_dbus_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      dbus::ObjectProxy::EmptyResponseCallback());
}

void SuspendManager::OnSuspendDoneHandled(dbus::Response* response) {
  VLOG(1) << "Received return of OnSuspendDoneHandled from BlueZ and NewBlue";

  is_suspend_operation_in_progress_ = false;

  if (suspend_id_) {
    // There was a SuspendImminent signal when we were unpausing discovery.
    // We should do the suspend preparation now.
    HandleSuspendImminent(suspend_id_);
  }
}

void SuspendManager::HandleSuspendImminent(int new_suspend_id) {
  // Update the current suspend id.
  suspend_id_ = new_suspend_id;

  // SuspendImminent/SuspendDone is in progress, just let it finish and
  // return early here.
  // If the in-progress call is SuspendImminent, when it finishes it will call
  // power manager HandleSuspendReadiness with the new updated suspend id.
  // If the in-progress call is SuspendDone, when it finishes it will
  // immediately initiate SuspendImminent again because suspend_id_ is now set.
  if (is_suspend_operation_in_progress_)
    return;

  is_suspend_operation_in_progress_ = true;

  dbus::MethodCall method_call(bluetooth_adapter::kBluetoothAdapterInterface,
                               bluetooth_adapter::kHandleSuspendImminent);

  VLOG(1) << "Calling SuspendImminent to BlueZ and NewBlue";
  btdispatch_dbus_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&SuspendManager::OnSuspendImminentHandled,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SuspendManager::HandleSuspendDone() {
  // Reset suspend_id_ to 0 before initiating the suspend preparation undo.
  // Needed to reflect that we are not in a suspend imminent state anymore.
  suspend_id_ = 0;

  // SuspendImminent/SuspendDone is in progress, just let it finish and
  // return early here.
  // If the in-progress call is SuspendImminent, when it finishes it will not
  // call HandleSuspendReadiness but will immediately initiate HandleSuspendDone
  // again because suspend_id_ is not set.
  if (is_suspend_operation_in_progress_)
    return;

  is_suspend_operation_in_progress_ = true;
  dbus::MethodCall method_call(bluetooth_adapter::kBluetoothAdapterInterface,
                               bluetooth_adapter::kHandleSuspendDone);

  VLOG(1) << "Calling HandleSuspendDone to BlueZ and NewBlue";
  btdispatch_dbus_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&SuspendManager::OnSuspendDoneHandled,
                 weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace bluetooth
