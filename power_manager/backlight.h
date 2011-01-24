// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_BACKLIGHT_H_
#define POWER_MANAGER_BACKLIGHT_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "power_manager/backlight_interface.h"
#include "power_manager/signal_callback.h"

typedef int gboolean;  // Forward declaration of bool type used by glib.

namespace power_manager {

// Get and set the brightness level of the display backlight.
//
// Example usage:
//   power_manager::Backlight backlight;
//   int64 level, max;
//   if (backlight.Init() && backlight.GetBrightness(&level, &max)) {
//     std::cout << "Current brightness level is "
//               << level << " out of " << max << "\n";
//   } else {
//     std::cout << "Cannot get brightness level\n";
//   }

class Backlight : public BacklightInterface {
 public:
  Backlight() {}
  virtual ~Backlight() {}

  // Initialize the backlight object.
  //
  // On success, return true; otherwise return false.
  bool Init();

  // Overridden from BacklightInterface:
  virtual bool GetBrightness(int64* level, int64* max);
  virtual bool GetTargetBrightness(int64* level);
  virtual bool SetBrightness(int64 level);

 private:
  // Look for the existence of required files and return the granularity of
  // the given backlight interface directory path.
  int64 CheckBacklightFiles(const FilePath& dir_path);

  // Directly and immediately set backlight brightness to a particular level,
  // without any gradual dimming.  The target_level argument is used to keep
  // track of what brightness level a given SetBrightnessHard command is being
  // used to move toward.
  //
  // Example:
  //   Current brightness = 40
  //   Want to set brightness to 60 over 5 steps, so the steps are:
  //      40 -> 44 -> 48 -> 52 -> 56 -> 60
  //   Thus, SetBrightnessHard(level, target_level) would be called five times
  //   with the args:
  //      SetBrightnessHard(44, 60);
  //      SetBrightnessHard(48, 60);
  //      SetBrightnessHard(52, 60);
  //      SetBrightnessHard(56, 60);
  //      SetBrightnessHard(60, 60);
  SIGNAL_CALLBACK_2(Backlight, gboolean, SetBrightnessHard, int64, int64);

  // Paths to the actual_brightness, brightness, and max_brightness files
  // under /sys/class/backlight.
  FilePath actual_brightness_path_;
  FilePath brightness_path_;
  FilePath max_brightness_path_;

  int64 target_brightness_;  // The current intended brightness level.

  DISALLOW_COPY_AND_ASSIGN(Backlight);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_BACKLIGHT_H_
