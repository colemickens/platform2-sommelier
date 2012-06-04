// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/power_manager_dbus_proxy.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "wimax_manager/power_manager.h"

using std::string;

namespace wimax_manager {

PowerManagerDBusProxy::PowerManagerDBusProxy(DBus::Connection *connection,
                                             PowerManager *power_manager)
    : DBusProxy(connection,
                power_manager::kPowerManagerServiceName,
                power_manager::kPowerManagerServicePath),
      power_manager_(power_manager) {
  CHECK(power_manager_);
}

PowerManagerDBusProxy::~PowerManagerDBusProxy() {
}

void PowerManagerDBusProxy::SuspendDelay(const uint32_t &sequence_number) {
  power_manager_->OnSuspendDelay(sequence_number);
}

void PowerManagerDBusProxy::PowerStateChanged(const string &new_power_state) {
  power_manager_->PowerStateChanged(new_power_state);
}

}  // namespace wimax_manager
