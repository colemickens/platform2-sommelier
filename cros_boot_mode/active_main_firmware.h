// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines the ActiveMainFirmware class which extracts the firmware volume used
// for boot.

#ifndef CROS_BOOT_MODE_ACTIVE_MAIN_FIRMWARE_H_
#define CROS_BOOT_MODE_ACTIVE_MAIN_FIRMWARE_H_

#include <sys/types.h>

#include "cros_boot_mode/platform_reader.h"

namespace cros_boot_mode {

class ActiveMainFirmware : public PlatformReader {
 public:
  enum {
    kRecovery = 0,
    kReadWriteA,
    kReadWriteB,
  };

  ActiveMainFirmware() = default;
  ~ActiveMainFirmware() override = default;

  static const char *kActiveMainFirmwareText[];
  static const size_t kActiveMainFirmwareCount;

  const char *name() const override;
  const char *c_str() const override;
  const char *default_platform_file_path() const override;
  size_t max_size() const override;
  int Process(const char *contents, size_t length) override;
};

}  // namespace cros_boot_mode

#endif  // CROS_BOOT_MODE_ACTIVE_MAIN_FIRMWARE_H_
