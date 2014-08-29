// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeveloperSwitch implementation

#include "cros_boot_mode/developer_switch.h"

namespace cros_boot_mode {

const char *DeveloperSwitch::name() const {
  return "developer";
}

unsigned int DeveloperSwitch::bitmask() const {
  return 0x00000020;  // specified in the firmware spec.
}

}  // namespace cros_boot_mode
