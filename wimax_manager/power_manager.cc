// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/power_manager.h"

#include <chromeos/dbus/service_constants.h>

#include "wimax_manager/dbus_control.h"
#include "wimax_manager/manager.h"
#include "wimax_manager/power_manager_dbus_proxy.h"

using std::string;

namespace wimax_manager {

namespace {

const uint32 kDefaultSuspendDelayInMilliSeconds = 5000;  // 5s

}  // namespace

PowerManager::PowerManager(Manager *wimax_manager)
    : suspend_delay_registered_(false),
      suspended_(false),
      wimax_manager_(wimax_manager) {
  CHECK(wimax_manager_);
}

PowerManager::~PowerManager() {
  Finalize();
}

void PowerManager::Initialize() {
  // TODO(benchan): May need to check if power manager is running and defer
  // the invocation of RegisterSuspendDelay when necessary.
  RegisterSuspendDelay(kDefaultSuspendDelayInMilliSeconds);
}

void PowerManager::Finalize() {
  UnregisterSuspendDelay();
}

void PowerManager::RegisterSuspendDelay(uint32 delay_ms) {
  if (!dbus_proxy())
    return;

  LOG(INFO) << "Register suspend delay of " << delay_ms << " ms.";
  try {
    dbus_proxy()->RegisterSuspendDelay(delay_ms);
    suspend_delay_registered_ = true;
  } catch (const DBus::Error &error) {
    LOG(ERROR) << "Failed to register suspend delay. DBus exception: "
               << error.name() << ": " << error.what();
  }
}

void PowerManager::UnregisterSuspendDelay() {
  if (!suspend_delay_registered_ || !dbus_proxy())
    return;

  LOG(INFO) << "Unregister suspend delay";
  try {
    dbus_proxy()->UnregisterSuspendDelay();
    suspend_delay_registered_ = false;
  } catch (const DBus::Error &error) {
    LOG(ERROR) << "Failed to unregister suspend delay. DBus exception: "
               << error.name() << ": " << error.what();
  }
}

void PowerManager::SuspendReady(uint32 sequence_number) {
  // TODO(benchan): Fix this code after SuspendReady is converted to a method
  // call on the PowerManager DBus interface.
  LOG(INFO) << "Signal suspend ready (" << sequence_number << ").";
  DBus::SignalMessage signal(power_manager::kPowerManagerServicePath,
                             power_manager::kPowerManagerInterface,
                             power_manager::kSuspendReady);
  DBus::MessageIter message = signal.writer();
  message << sequence_number;
  if (!DBusControl::GetConnection()->send(signal)) {
    LOG(ERROR) << "Failed to signal suspend ready (" << sequence_number << ").";
  }
}

void PowerManager::OnSuspendDelay(uint32 sequence_number) {
  LOG(INFO) << "Received suspend delay (seqno " << sequence_number << ").";
  if (!suspended_) {
    wimax_manager_->Suspend();
    suspended_ = true;
  }
  SuspendReady(sequence_number);
}

void PowerManager::PowerStateChanged(const std::string &new_power_state) {
  LOG(INFO) << "Power state changed to '" << new_power_state << "'.";
  if (suspended_ && new_power_state == "on") {
    wimax_manager_->Resume();
    suspended_ = false;
  }
}

}  // namespace wimax_manager
