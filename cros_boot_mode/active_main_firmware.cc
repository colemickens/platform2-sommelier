// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ActiveMainFirmware implementation

#include "active_main_firmware.h"

#include <sys/types.h>

#include "helpers.h"

namespace cros_boot_mode {

const char *ActiveMainFirmware::kActiveMainFirmwareText[] = {
 "recovery",
 "a",
 "b",
};

const char *ActiveMainFirmware::c_str() const {
  return (value() >= 0 ? kActiveMainFirmwareText[value()] : "unsupported");
}

const size_t ActiveMainFirmware::kActiveMainFirmwareCount =
  sizeof(kActiveMainFirmwareText) / sizeof(*kActiveMainFirmwareText);

ActiveMainFirmware::ActiveMainFirmware() { }
ActiveMainFirmware::~ActiveMainFirmware() { }

int ActiveMainFirmware::Process(const char *contents, size_t length) {
  int volume = helpers::to_int(contents, length);
  if (volume >= 0 &&
      volume < static_cast<int>(kActiveMainFirmwareCount))
    return volume;
  return kUnsupported;
}

}  // namespace cros_boot_mode
