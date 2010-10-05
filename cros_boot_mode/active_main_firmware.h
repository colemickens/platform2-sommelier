// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines the ActiveMainFirmware class which extracts the firmware volume used
// for boot.
#ifndef CROS_BOOT_MODE_ACTIVE_MAIN_FIRMWARE_H_
#define CROS_BOOT_MODE_ACTIVE_MAIN_FIRMWARE_H_

#include <sys/types.h>

#include "platform_reader.h"

namespace cros_boot_mode {

class ActiveMainFirmware : public PlatformReader {
 public:
  ActiveMainFirmware();
  virtual ~ActiveMainFirmware();

  enum {
    kRecovery = 0,
    kReadWriteA,
    kReadWriteB,
  };
  static const char *kActiveMainFirmwareText[];
  static const size_t kActiveMainFirmwareCount;

  virtual const char *c_str() const;
  virtual int Process(const char *contents, size_t length);
  virtual const char *default_platform_file_path() const {
    return "/sys/devices/platform/chromeos_acpi/BINF.1";
  }
  virtual const char *name() const {
    return "active_main_firmware";
  }
  virtual size_t max_size() const {
    return sizeof("-1");
  }
};

}  // namespace cros_boot_mode
#endif  // CROS_BOOT_MODE_ACTIVE_MAIN_FIRMWARE_H_
