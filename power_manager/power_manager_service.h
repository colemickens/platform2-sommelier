// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_MANAGER_SERVICE_H_
#define POWER_MANAGER_POWER_MANAGER_SERVICE_H_

#include "base/basictypes.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

namespace power_manager {

class Daemon;

class PowerManagerService {
 public:
  explicit PowerManagerService(Daemon* daemon);

  bool Register(DBusConnection* conn);

 private:
  Daemon* daemon_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerService);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_MANAGER_SERVICE_H_
