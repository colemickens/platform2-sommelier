// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// boot_mode uses BINF.0, BINF.1, and CHSW to provide a confident guess as
// to the current boot state.
#ifndef CROS_BOOT_MODE_BOOT_MODE_H_
#define CROS_BOOT_MODE_BOOT_MODE_H_

#include <stdio.h>

#include "active_main_firmware.h"
#include "bootloader_type.h"
#include "developer_switch.h"

namespace cros_boot_mode {

class BootMode {
 public:
  BootMode();
  virtual ~BootMode();

  enum Mode {
    kUnsupported = -1,
    kNormal = 0,
    kDeveloper,
    // Recovery modes below this line.
    kNormalRecovery,
    kDeveloperRecovery,
  };

  // Initializes the class by reading from the platform-specific
  // implementation.  Even if something fails, the class will be in
  // a valid state, but the system will appear to be unsupported.
  virtual void Initialize(bool unsupported_is_developer, bool use_bootloader);
  // Returns a string of [mode][space][modifier(s)]
  virtual const char *mode_text() const {
    return (mode() >= 0 ? kBootModeText[mode()] : "unsupported");
  }
  virtual Mode mode() const {
    return mode_;
  }
  virtual bool recovery() const {
    return (mode() >= kNormalRecovery);
  }

  // Overrides for swapping out dependencies
  // Pointer ownership is not taken in any case.
  virtual void set_developer_switch(DeveloperSwitch *ds);
  virtual void set_active_main_firmware(ActiveMainFirmware *amf);
  virtual void set_bootloader_type(BootloaderType *bt);
  virtual const DeveloperSwitch *developer_switch() const {
    return developer_switch_;
  }
  virtual const ActiveMainFirmware *active_main_firmware() const {
    return active_main_firmware_;
  }
  virtual const BootloaderType *bootloader_type() const {
    return bootloader_type_;
  }

 private:
  static const char *kBootModeText[];
  static const size_t kBootModeCount;
  DeveloperSwitch default_developer_switch_;
  ActiveMainFirmware default_active_main_firmware_;
  BootloaderType default_bootloader_type_;
  Mode mode_;
  DeveloperSwitch *developer_switch_;
  ActiveMainFirmware *active_main_firmware_;
  BootloaderType *bootloader_type_;
};

}  // namespace cros_boot_mode

#endif  // CROS_BOOT_MODE_BOOT_MODE_H_
