// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/power-manager-proxy.h"

#include <chromeos/dbus/service_constants.h>

#include "cros-disks/power-manager-observer-interface.h"

namespace cros_disks {

PowerManagerProxy::PowerManagerProxy(DBus::Connection* connection)
    : DBus::InterfaceProxy(power_manager::kPowerManagerInterface),
      DBus::ObjectProxy(*connection, "/",
                        power_manager::kPowerManagerServiceName) {
  connect_signal(PowerManagerProxy, ScreenIsLocked, OnScreenIsLocked);
  connect_signal(PowerManagerProxy, ScreenIsUnlocked, OnScreenIsUnlocked);
}

PowerManagerProxy::~PowerManagerProxy() {
}

void PowerManagerProxy::AddObserver(PowerManagerObserverInterface* observer) {
  CHECK(observer) << "Invalid observer object";
  observer_list_.AddObserver(observer);
}

void PowerManagerProxy::OnScreenIsLocked(const DBus::SignalMessage& signal) {
  FOR_EACH_OBSERVER(PowerManagerObserverInterface, observer_list_,
                    OnScreenIsLocked());
}

void PowerManagerProxy::OnScreenIsUnlocked(const DBus::SignalMessage& signal) {
  FOR_EACH_OBSERVER(PowerManagerObserverInterface, observer_list_,
                    OnScreenIsUnlocked());
}

}  // namespace cros_disks
