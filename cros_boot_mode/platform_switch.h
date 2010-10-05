// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides a default implementation of a platform switch.  On CrOS, switch
// state is represented in an integer exposed via a file in /sys.  Each
// switch recieves one bit which is checked with a subclass-defined
// bitmask().
//
// A subclass only needs to set its name and bitmask to "just work".

#ifndef CROS_BOOT_MODE_PLATFORM_SWITCH_H_
#define CROS_BOOT_MODE_PLATFORM_SWITCH_H_

#include "platform_reader.h"

namespace cros_boot_mode {

class PlatformSwitch : public PlatformReader {
 public:
  PlatformSwitch();
  virtual ~PlatformSwitch();

  enum { kDisabled, kEnabled };
  static const char *kPositionText[];

  virtual const char *c_str() const;
  virtual const char *default_platform_file_path() const {
    return "/sys/devices/platform/chromeos_acpi/CHSW";
  }
  virtual size_t max_size() const {
    return sizeof("65535");  // largest allowed switch value
  }
  virtual int Process(const char *file_contents, size_t length);

  // To be overriden by the implementation
  virtual unsigned int bitmask() const = 0;
  virtual const char *name() const = 0;
};

}  // namespace cros_boot_mode
#endif  // CROS_BOOT_MODE_PLATFORM_SWITCH_H_
