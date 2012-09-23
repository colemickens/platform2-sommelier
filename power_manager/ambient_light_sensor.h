// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_AMBIENT_LIGHT_SENSOR_H_
#define POWER_MANAGER_AMBIENT_LIGHT_SENSOR_H_

#include <glib.h>

#include "power_manager/async_file_reader.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/power_constants.h"
#include "power_manager/power_prefs_interface.h"
#include "power_manager/signal_callback.h"

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
//   // SetAlsBrightnessOffsetPercent() method:
//   void BacklightController::SetAlsBrightnessOffsetPercent(double percent) {
//     LOG(INFO) << "Light sensor sets an als offset of " << percent
//   }

class AmbientLightSensor {
 public:
  explicit AmbientLightSensor(BacklightController* controller,
                              PowerPrefsInterface* prefs);
  ~AmbientLightSensor();

  // Initialize the AmbientLightSensor object.
  // Open the lux file so we can read ambient light data.
  // On success, return true; otherwise return false.
  bool Init();

  // The backlight controller sends us power state events so we can
  // enable and disable polling.
  void EnableOrDisableSensor(PowerState state);

 private:
  // Handler for a periodic event that reads the ambient light sensor.
  SIGNAL_CALLBACK_0(AmbientLightSensor, gboolean, ReadAls);

  // Asynchronous I/O success and error handlers, respectively.
  void ReadCallback(const std::string& data);
  void ErrorCallback();

  // Deferred init for the als in case the light sensor starts late.
  bool DeferredInit();

  // Return a luma level normalized to 100 based on the tsl2563 lux value.
  // The luma level will modify the controller's brightness calculation.
  double Tsl2563LuxToPercent(int luxval);

  // Use this to send AmbientLightSensor events to BacklightController.
  BacklightController* controller_;

  // Interface for saving preferences. Non-owned.
  PowerPrefsInterface* prefs_;

  // These flags are used to turn on and off polling.
  bool is_polling_;
  bool disable_polling_;

  // Issue reasonable diagnostics about the deferred lux file open.
  // Flag indicating whether a valid ALS device lux value file has been found.
  bool still_deferring_;

  // Flag indicating whether a valid ALS device lux value file has been found.
  bool als_found_;

  // This is the ambient light sensor asynchronous file I/O object.
  AsyncFileReader als_file_;

  // These are used in the LuxToPercent calculation.
  double log_multiply_factor_;
  double log_subtract_factor_;

  // Callbacks for asynchronous file I/O.
  base::Callback<void(const std::string&)> read_cb_;
  base::Callback<void()> error_cb_;


  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensor);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_AMBIENT_LIGHT_SENSOR_H_
