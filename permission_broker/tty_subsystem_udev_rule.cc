// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/tty_subsystem_udev_rule.h"

#include <libudev.h>

#include <string>

using std::string;

namespace permission_broker {

TtySubsystemUdevRule::TtySubsystemUdevRule(const string& name)
    : UdevRule(name) {}

Rule::Result TtySubsystemUdevRule::ProcessDevice(udev_device* device) {
  const char* const subsystem = udev_device_get_subsystem(device);
  if (!subsystem || strcmp(subsystem, "tty"))
    return IGNORE;
  return ProcessTtyDevice(device);
}

}  // namespace permission_broker
