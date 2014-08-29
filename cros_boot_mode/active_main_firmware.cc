// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ActiveMainFirmware implementation

#include "cros_boot_mode/active_main_firmware.h"

#include <sys/types.h>

#include "cros_boot_mode/helpers.h"

namespace cros_boot_mode {

const char *ActiveMainFirmware::kActiveMainFirmwareText[] = {
  "recovery",
  "a",
  "b",
};

const size_t ActiveMainFirmware::kActiveMainFirmwareCount =
  sizeof(kActiveMainFirmwareText) / sizeof(*kActiveMainFirmwareText);

const char *ActiveMainFirmware::name() const {
  return "active_main_firmware";
}

const char *ActiveMainFirmware::c_str() const {
  return (value() >= 0 ? kActiveMainFirmwareText[value()] : "unsupported");
}

const char *ActiveMainFirmware::default_platform_file_path() const {
  return "/sys/devices/platform/chromeos_acpi/BINF.1";
}

size_t ActiveMainFirmware::max_size() const {
  return sizeof("-1");
}

int ActiveMainFirmware::Process(const char *contents, size_t length) {
  int volume = helpers::to_int(contents, length);
  if (volume >= 0 && volume < static_cast<int>(kActiveMainFirmwareCount))
    return volume;
  return kUnsupported;
}

}  // namespace cros_boot_mode
