// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_BACKLIGHT_CONTROLLER_H_

#include <algorithm>

#include "power_manager/backlight_interface.h"
#include "power_manager/xidle_monitor.h"

namespace power_manager {

enum BacklightState {
  BACKLIGHT_ACTIVE, BACKLIGHT_DIM
};

// Control the backlight.
class BacklightController {
 public:
  explicit BacklightController(BacklightInterface* backlight);
  virtual ~BacklightController() {}

  // Initialize the object.
  bool Init();

  // Set |level| to the current brightness level of the backlight, and set
  // |max| to the max brightness level of the backlight. The minimum
  // brightness level of the backlight is zero.
  void GetBrightness(int64* level, int64* max);

  // Increase / decrease brightness by specified offset
  void ChangeBrightness(int64 offset);

  // Set the state of the backlight to active or dim
  void SetBacklightState(BacklightState state);

  // Mark the computer as plugged or unplugged, and adjust the brightness
  // appropriately. Before calling this method, make sure to call the
  // set_plugged_brightness_offset and set_unplugged_brightness_offset
  // methods below.
  void OnPlugEvent(bool is_plugged);

  // Read brightness settings from the system and apply any changes made
  // by other programs to our local view.
  int64 ReadBrightness();

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
  // Clamp |x| to fit between min_ and max_.
  int64 clamp(int64 x) { return std::min(max_, std::max(min_, x)); }

  // Backlight used for dimming. Non-owned.
  BacklightInterface* backlight_;

  // Offsets used to calculate desired brightness.
  int64 als_brightness_level_;
  int64 plugged_brightness_offset_;
  int64 unplugged_brightness_offset_;

  // Pointer to currently in-use brightness offset
  int64* brightness_offset_;

  // Whether backlight is active, dimmed, or off
  BacklightState state_;

  // Whether the computer is plugged in.
  enum {
    POWER_DISCONNECTED, POWER_CONNECTED, POWER_UNKNOWN
  } plugged_state_;

  // Current system brightness.
  int64 system_brightness_;

  // Min and max brightness for clamp.
  int64 min_;
  int64 max_;

  DISALLOW_COPY_AND_ASSIGN(BacklightController);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_BACKLIGHT_CONTROLLER_H_
