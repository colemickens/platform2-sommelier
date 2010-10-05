// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Concrete class for DeveloperSwitch.  This class represents the physical
// developer mode switch on the Chrome OS device and maps its value to
// kEnabled, kDisabled, and kUnsupported as per PlatformSwitch.
//
// The developer mode switch shares a file in /sys which is provided by
// chromeos_acpi for the x86 plaform (ARM TBD).  The file contains an integer
// in ASCII format (-1, 0, 544, whatever) with one bit representing a
// platform switch value.  The developer switch is allocated the 6th
// bit (0x20).  The file in use (CHSW) indicates the boot-time position
// of the switch and not a current measurement (available in GPIO.* files).
#ifndef CROS_BOOT_MODE_DEVELOPER_SWITCH_H_
#define CROS_BOOT_MODE_DEVELOPER_SWITCH_H_

#include "platform_switch.h"

namespace cros_boot_mode {

class DeveloperSwitch : public PlatformSwitch {
 public:
  DeveloperSwitch();
  virtual ~DeveloperSwitch();
  virtual const char *name() const {
    return "developer";
  }
  virtual unsigned int bitmask() const {
    return 0x00000020;  // specified in the firmware spec.
  }
};

}  // namespace cros_boot_mode
#endif  // CROS_BOOT_MODE_DEVELOPER_SWITCH_H_
