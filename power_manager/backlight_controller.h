// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_BACKLIGHT_CONTROLLER_H_

#include <algorithm>

#include "power_manager/backlight_interface.h"
#include "power_manager/power_prefs_interface.h"

namespace power_manager {

enum DimState {
  BACKLIGHT_ACTIVE, BACKLIGHT_DIM
};

enum PowerState {
  BACKLIGHT_OFF, BACKLIGHT_ON
};

// Control the backlight.
class BacklightController {
 public:
  explicit BacklightController(BacklightInterface* backlight,
                               PowerPrefsInterface* prefs);
  virtual ~BacklightController() {}

  // Initialize the object.
  bool Init();

  // Set |level| to the current brightness level of the backlight as a
  // percentage.
  void GetBrightness(int64* level);

  // Increase the brightness level of the backlight by one level.
  void IncreaseBrightness();

  // Decrease the brightness level of the backlight by one level.
  void DecreaseBrightness();

  // Set the backlight to active or dim.
  void SetDimState(DimState state);

  // Turn the backlight on or off
  void SetPowerState(PowerState state);

  // Mark the computer as plugged or unplugged, and adjust the brightness
  // appropriately.
  void OnPlugEvent(bool is_plugged);

  // Read brightness settings from the system and apply any changes made
  // by other programs to our local view. Return true if the brightness
  // has not been modified by other programs; return false otherwise.
  bool ReadBrightness();

  // Write brightness based on current settings. Returns new brightness level.
  int64 WriteBrightness();

  void set_als_brightness_level(int64 level) { als_brightness_level_ = level; }

  int64 plugged_brightness_offset() { return plugged_brightness_offset_; }
  void set_plugged_brightness_offset(int64 offset) {
    plugged_brightness_offset_ = offset;
  }

  int64 unplugged_brightness_offset() { return unplugged_brightness_offset_; }
  void set_unplugged_brightness_offset(int64 offset) {
    unplugged_brightness_offset_ = offset;
  }

 private:
  // Clamp |x| to fit between 0 and 100.
  int64 clamp(int64 x) { return std::min(100LL, std::max(0LL, x)); }

  void ReadPrefs();
  void WritePrefs();

  // Backlight used for dimming. Non-owned.
  BacklightInterface* backlight_;

  // Interface for saving preferences. Non-owned.
  PowerPrefsInterface* prefs_;

  // Offsets used to calculate desired brightness.
  int64 als_brightness_level_;
  int64 plugged_brightness_offset_;
  int64 unplugged_brightness_offset_;

  // Pointer to currently in-use brightness offset
  int64* brightness_offset_;

  // Whether backlight is active or dimmed
  DimState state_;

  // Whether the computer is plugged in.
  enum {
    POWER_DISCONNECTED, POWER_CONNECTED, POWER_UNKNOWN
  } plugged_state_;

  // Current system brightness.
  int64 system_brightness_;

  // Min and max brightness for backlight object.
  int64 min_;
  int64 max_;

  DISALLOW_COPY_AND_ASSIGN(BacklightController);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_BACKLIGHT_CONTROLLER_H_
