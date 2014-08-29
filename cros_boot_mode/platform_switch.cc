// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implements PlatformSwitch by reading in the switch file (CHSW) and converting
// its contents to an unsigned integer which is bitwise-ANDed with the
// subclass-defined bitmask().

#include "cros_boot_mode/platform_switch.h"

#include "cros_boot_mode/helpers.h"

namespace cros_boot_mode {

const char *PlatformSwitch::kPositionText[] = { "disabled", "enabled" };

const char *PlatformSwitch::c_str() const {
  return (value() >= 0 ? kPositionText[value()] : "unsupported");
}

const char *PlatformSwitch::default_platform_file_path() const {
  return "/sys/devices/platform/chromeos_acpi/CHSW";
}

size_t PlatformSwitch::max_size() const {
  return sizeof("65535");  // largest allowed switch value
}

int PlatformSwitch::Process(const char *file_contents, size_t length) {
  int read_int = helpers::to_int(file_contents, length);
  if (read_int < 0)
    return kUnsupported;
  unsigned int position = static_cast<unsigned int>(read_int);
  return !((position & bitmask()) == 0);
}

}  // namespace cros_boot_mode
