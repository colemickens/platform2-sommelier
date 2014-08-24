// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/power_manager_dbus_proxy.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>
#include <google/protobuf/message_lite.h>

#include "power_manager/proto_bindings/suspend.pb.h"
#include "wimax_manager/power_manager.h"

using std::string;
using std::vector;

namespace wimax_manager {

PowerManagerDBusProxy::PowerManagerDBusProxy(DBus::Connection *connection,
                                             PowerManager *power_manager)
    : DBusProxy(connection,
                power_manager::kPowerManagerServiceName,
                power_manager::kPowerManagerServicePath),
      power_manager_(power_manager) {
  CHECK(power_manager_);
}

void PowerManagerDBusProxy::SuspendImminent(
    const vector<uint8_t> &serialized_proto) {
  power_manager_->OnSuspendImminent(serialized_proto);
}

void PowerManagerDBusProxy::SuspendDone(
    const vector<uint8_t> &serialized_proto) {
  power_manager_->OnSuspendDone(serialized_proto);
}

}  // namespace wimax_manager
