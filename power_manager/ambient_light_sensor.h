// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_AMBIENT_LIGHT_SENSOR_H_
#define POWER_MANAGER_AMBIENT_LIGHT_SENSOR_H_

#include <glib.h>

#include "power_manager/backlight_controller.h"

namespace power_manager {

// Get ambient light sensor data and feed it into the backlight interface.
//
// Example usage:
//   // Initialization:
//   power_manager::BacklightController backlight_ctl(&backlight, &prefs);
//   CHECK(backlight_ctl.Init()) << "fatal";
//   power_manager::AmbientLightSensor als(&backlight_ctl);
//   if (!als.Init())
//     LOG(WARNING) << "not fatal, but we get no light sensor events";
//
//   // BacklightController enables or disables the light sensor as the
//   // backlight power state changes:
//   backlight_ctl.light_sensor_->EnableOrDisableSensor(BACKLIGHT_ON,
//                                                      BACKLIGHT_ACTIVE);
//
//   // BacklightController receives light sensor events in its
//   // SetAlsBrightnessLevel() method:
//   void BacklightController::SetAlsBrightnessLevel(int64 level) {
//     LOG(INFO) << "Light sensor sets an als offset of " << level;
//   }

class AmbientLightSensor {
 public:
  explicit AmbientLightSensor(BacklightController* controller);
  ~AmbientLightSensor();

  // Initialize the AmbientLightSensor object.
  // Open the lux file so we can read ambient light data.
  // On success, return true; otherwise return false.
  bool Init();

  // The backlight controller sends us power state events so we can
  // enable and disable polling.
  void EnableOrDisableSensor(PowerState power, DimState dim);

 private:
  static gboolean ReadAls(gpointer data);

  // Return a luma level normalized to 100 based on the tsl2563 lux value.
  // The luma level will modify the controller's brightness calculation.
  static int64 Tsl2563LuxToLevel(int luxval);

  // Use this to send AmbientLightSensor events to BacklightController.
  BacklightController* controller_;

  // This is the ambient light sensor file descriptor.
  int als_fd_;

  // Record the last read ambient light level to reduce unnecessary events.
  int64 last_level_;

  // These flags are used to turn on and off polling.
  bool is_polling_;
  bool disable_polling_;

  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensor);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_AMBIENT_LIGHT_SENSOR_H_
