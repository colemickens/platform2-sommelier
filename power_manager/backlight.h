// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_BACKLIGHT_H_
#define POWER_MANAGER_BACKLIGHT_H_

#include "base/basictypes.h"
#include "base/file_path.h"

namespace power_manager {

// \brief Interface for getting and setting the level of the backlight
class BacklightInterface {
 public:
  // Set *level to the current brightness level of the backlight, and set
  // *max to the max brightness level of the backlight. The minimum brightness
  // level of the backlight is zero.
  //
  // On success, return true; otherwise return false.
  virtual bool GetBrightness(int64* level, int64* max) = 0;

  // Set the backlight to the specified brightness level.
  //
  // On success, return true; otherwise return false.
  virtual bool SetBrightness(int64 level) = 0;

 protected:
  ~BacklightInterface() { }
};

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

class Backlight : public BacklightInterface {
 public:
  Backlight() { }

  // Initialize the backlight object.
  //
  // On success, return true; otherwise return false.
  bool Init();

  bool GetBrightness(int64* level, int64* max);
  bool SetBrightness(int64 level);

 private:
  // Paths to the actual_brightness, brightness, and max_brightness files
  // under /sys/class/backlight
  FilePath actual_brightness_path_, brightness_path_, max_brightness_path_;
  DISALLOW_COPY_AND_ASSIGN(Backlight);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_BACKLIGHT_H_
