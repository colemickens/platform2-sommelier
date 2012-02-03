// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_BACKLIGHT_H_
#define POWER_MANAGER_BACKLIGHT_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "power_manager/backlight_interface.h"
#include "power_manager/signal_callback.h"

namespace power_manager {

// Controls an LCD backlight via sysfs.
class Backlight : public BacklightInterface {
 public:
  Backlight();
  virtual ~Backlight() {}

  // Initialize the backlight object.
  //
  // The |base_path| specifies the directory to look for backlights.  The
  // |pattern| is a glob pattern to help find the right backlight.
  // Expected values for parameters look like:
  //   base: "/sys/class/backlight", pattern: "*"
  //   base: "/sys/class/leds", pattern: "*:kbd_backlight"
  //
  // On success, return true; otherwise return false.
  bool Init(const FilePath& base_path, const FilePath::StringType& pattern);

  // Overridden from BacklightInterface:
  virtual bool GetMaxBrightnessLevel(int64* max_level);
  virtual bool GetCurrentBrightnessLevel(int64* current_level);
  virtual bool SetBrightnessLevel(int64 level);

 private:
  // Generate FilePaths within |dir_path| for reading and writing brightness
  // information.
  static void GetBacklightFilePaths(const FilePath& dir_path,
                                    FilePath* actual_brightness_path,
                                    FilePath* brightness_path,
                                    FilePath* max_brightness_path);

  // Look for the existence of required files and return the max brightness.
  // Returns 0 if necessary files are missing.
  static int64 CheckBacklightFiles(const FilePath& dir_path);

  // Read the value from |path| to |level|.  Returns false on failure.
  static bool ReadBrightnessLevelFromFile(const FilePath& path, int64* level);

  // Paths to the actual_brightness, brightness, and max_brightness files
  // under /sys/class/backlight.
  FilePath actual_brightness_path_;
  FilePath brightness_path_;
  FilePath max_brightness_path_;

  // Cached maximum brightness level.
  int64 max_brightness_level_;

  DISALLOW_COPY_AND_ASSIGN(Backlight);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_BACKLIGHT_H_
