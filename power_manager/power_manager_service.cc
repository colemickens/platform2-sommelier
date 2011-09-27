// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_manager_service.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/powerd.h"

namespace power_manager {

PowerManagerService::PowerManagerService(Daemon* daemon)
    : daemon_(daemon) {
}

bool PowerManagerService::Register(DBusConnection* connection) {
  CHECK(connection);
  DBusError error;
  dbus_error_init(&error);
  if (dbus_bus_request_name(connection,
                            power_manager::kPowerManagerServiceName,
                            0,
                            &error) < 0) {
    LOG(FATAL) << "Failed to register name \""
               << power_manager::kPowerManagerServiceName << "\": "
               << (dbus_error_is_set(&error) ? error.message : "Unknown error");
  }

  // Relying on the existing message filters of powerd and powerman
  // to handle all of their signals. Specifically-targetted messages
  // are delivered to this object.

  // TODO(cwolfe): Daemon includes both D-Bus functionality and logic for
  // routing operations to its various subsystems. This service class provides
  // an opportunity to (gradually) refactor the D-Bus calls out of Daemon,
  // simplifying it in the process. Will file this as a low-priority cleanup.

  return true;
}

}  // namespace power_manager
