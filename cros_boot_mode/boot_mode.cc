// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Relies on underlying platform classes to determine the system state.
// In particular, this class orders the current active firmware,
// developer mode switch value, and the bootloader kernel commandline
// boot parameter to determine the "boot mode".

#include <sys/types.h>

#include "active_main_firmware.h"
#include "bootloader_type.h"
#include "developer_switch.h"

#include "boot_mode.h"

namespace cros_boot_mode {

const char *BootMode::kBootModeText[] = {
 "normal",
 "developer",
 "normal recovery",
 "developer recovery",
};
const size_t BootMode::kBootModeCount =
  sizeof(kBootModeText) / sizeof(*kBootModeText);

BootMode::BootMode() :
  mode_(kUnsupported),
  developer_switch_(&default_developer_switch_),
  active_main_firmware_(&default_active_main_firmware_),
  bootloader_type_(&default_bootloader_type_) { }
BootMode::~BootMode()  { }

void BootMode::Initialize(bool unsupported_is_developer, bool use_bootloader) {
  if (use_bootloader) {
    // For now, we treat the bootloader mode as priority over the
    // firmware-supplied values.  This allows for the kernel commandline to
    // allow testing different behaviors changing only the kernel and not
    // the switch positions or system image.
    bootloader_type_->Initialize();
    // TODO(wad) potentially create a standalone debug mode. TBD.
    if (bootloader_type_->value() == BootloaderType::kDebug) {
      mode_ = kDeveloper;
      return;
    }
    if (bootloader_type_->value() != BootloaderType::kChromeOS) {
      if (unsupported_is_developer)
        mode_ = kDeveloper;
      return;
    }
  }

  // After the bootloader, the developer switch position guides the primary
  // decision around current mode.
  developer_switch_->Initialize();
  switch (developer_switch_->value()) {
    case PlatformSwitch::kEnabled:
      mode_ = kDeveloper;
      break;
    case PlatformSwitch::kDisabled:
      mode_ = kNormal;
      break;
    default:
      // If we're using the bootloader, rely on it if there's no dev switch.
      if (use_bootloader)
        return;
      // Otherwise, a missing dev switch can be mapped to developer.
      if (unsupported_is_developer)
        mode_ = kDeveloper;
      return;
  }

  // The sub-mode of "recovery" can be determined by checking if the firmware
  // booted via the recovery firmware.
  active_main_firmware_->Initialize();
  if (active_main_firmware_->value() == ActiveMainFirmware::kRecovery) {
    if (mode_ == kNormal) {
      mode_ = kNormalRecovery;
    } else if (mode_ == kDeveloper) {
      mode_ = kDeveloperRecovery;
    }
  }
  return;
}


void BootMode::set_developer_switch(DeveloperSwitch *ds) {
  if (ds)
    developer_switch_ = ds;
  else
    developer_switch_ = &default_developer_switch_;
}

void BootMode::set_active_main_firmware(ActiveMainFirmware *amf) {
 if (amf)
   active_main_firmware_ = amf;
  else
   active_main_firmware_ = &default_active_main_firmware_;
}

void BootMode::set_bootloader_type(BootloaderType *bt) {
  if (bt)
    bootloader_type_ = bt;
  else
    bootloader_type_ = &default_bootloader_type_;
}

}  // namespace cros_boot_mode
