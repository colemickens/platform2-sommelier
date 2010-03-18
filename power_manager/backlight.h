// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_PLATFORM_POWER_MANAGER_BACKLIGHT_H_
#define SRC_PLATFORM_POWER_MANAGER_BACKLIGHT_H_

#include "base/basictypes.h"
#include "base/file_path.h"

namespace power_manager {

// \brief Get and set the brightness level of the display backlight.
//
// \example
// power_manager::Backlight backlight;
// int64 level, max;
// if (backlight.Init() && backlight.GetBrightness(&level, &max)) {
//   std::cout << "Current brightness level is "
//             << level << " out of " << max << "\n";
// } else {
//   std::cout << "Cannot get brightness level\n";
// }
// \end_example

class Backlight {
 public:
  Backlight() { }

  // Initialize the backlight object.
  //
  // On success, return true; otherwise return false.
  bool Init();

  // Set *level to the current brightness level of the backlight, and set
  // *max to the max brightness level of the backlight. The minimum brightness
  // level of the backlight is zero.
  //
  // On success, return true; otherwise return false.
  bool GetBrightness(int64* level, int64* max);

  // Set the backlight to the specified brightness level.
  //
  // On success, return true; otherwise return false.
  bool SetBrightness(int64 level);

 private:

  FilePath brightness_path_, max_brightness_path_;

  DISALLOW_COPY_AND_ASSIGN(Backlight);
};

}  // namespace power_manager

#endif  // SRC_PLATFORM_POWER_MANAGER_BACKLIGHT_H_
