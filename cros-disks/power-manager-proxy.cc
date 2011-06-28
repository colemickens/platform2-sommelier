// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/power-manager-proxy.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "cros-disks/power-manager-observer.h"

namespace cros_disks {

PowerManagerProxy::PowerManagerProxy(DBus::Connection* connection,
                                     PowerManagerObserver* observer)
    : DBus::InterfaceProxy(power_manager::kPowerManagerInterface),
      DBus::ObjectProxy(*connection, "/",
                        power_manager::kPowerManagerServiceName),
      observer_(observer) {
  CHECK(observer_) << "Invalid power manager observer";
  connect_signal(PowerManagerProxy, ScreenIsLocked, OnScreenIsLocked);
  connect_signal(PowerManagerProxy, ScreenIsUnlocked, OnScreenIsUnlocked);
}

PowerManagerProxy::~PowerManagerProxy() {
}

void PowerManagerProxy::OnScreenIsLocked(const DBus::SignalMessage& signal) {
  observer_->OnScreenIsLocked();
}

void PowerManagerProxy::OnScreenIsUnlocked(const DBus::SignalMessage& signal) {
  observer_->OnScreenIsUnlocked();
}

}  // namespace cros_disks
