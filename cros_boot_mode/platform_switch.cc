// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implements PlatformSwitch by reading in the switch file (CHSW) and converting
// its contents to an unsigned integer which is bitwise-ANDed with the
// subclass-defined bitmask().

#include "platform_switch.h"

#include "helpers.h"

namespace cros_boot_mode {

const char *PlatformSwitch::kPositionText[] = { "disabled", "enabled" };

PlatformSwitch::PlatformSwitch() { }
PlatformSwitch::~PlatformSwitch() { }

const char *PlatformSwitch::c_str() const {
  return (value() >= 0 ? kPositionText[value()] : "unsupported");
}

int PlatformSwitch::Process(const char *file_contents, size_t length) {
  int read_int = helpers::to_int(file_contents, length);
  if (read_int < 0)
    return kUnsupported;
  unsigned int position = static_cast<unsigned int>(read_int);
  return !((position & bitmask()) == 0);
}

}  // namespace cros_boot_mode
