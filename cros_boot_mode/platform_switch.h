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

#include "cros_boot_mode/platform_reader.h"

namespace cros_boot_mode {

class PlatformSwitch : public PlatformReader {
 public:
  enum { kDisabled, kEnabled };

  PlatformSwitch() = default;
  ~PlatformSwitch() override = default;

  const char *c_str() const override;
  const char *default_platform_file_path() const override;
  size_t max_size() const override;
  int Process(const char *file_contents, size_t length) override;

  // To be overriden by the implementation
  virtual unsigned int bitmask() const = 0;
  virtual const char *name() const = 0;

  static const char *kPositionText[];
};

}  // namespace cros_boot_mode

#endif  // CROS_BOOT_MODE_PLATFORM_SWITCH_H_
