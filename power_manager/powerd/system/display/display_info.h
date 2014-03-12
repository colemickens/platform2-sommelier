// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_DISPLAY_DISPLAY_INFO_H_
#define POWER_MANAGER_POWERD_SYSTEM_DISPLAY_DISPLAY_INFO_H_

#include <base/files/file_path.h>

namespace power_manager {
namespace system {

// Information about a connected display.
struct DisplayInfo {
 public:
  DisplayInfo();
  ~DisplayInfo();

  bool operator<(const DisplayInfo& rhs) const;
  bool operator==(const DisplayInfo& o) const;

  // Path to the directory in /sys representing the DRM device connected to this
  // display.
  base::FilePath drm_path;

  // Path to the I2C device in /dev that can be used to communicate with this
  // display.
  base::FilePath i2c_path;
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_DISPLAY_DISPLAY_INFO_H_
