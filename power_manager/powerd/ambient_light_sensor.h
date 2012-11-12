// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_AMBIENT_LIGHT_SENSOR_H_
#define POWER_MANAGER_POWERD_AMBIENT_LIGHT_SENSOR_H_

#include <glib.h>
#include <list>
#include <string>

#include "base/observer_list.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/async_file_reader.h"

namespace power_manager {

class AmbientLightSensor;

class AmbientLightSensorObserver {
 public:
  virtual void OnAmbientLightChanged(AmbientLightSensor* sensor) = 0;
};

class AmbientLightSensor {
 public:
  AmbientLightSensor();
  virtual ~AmbientLightSensor();

  // Adds or removes observers for sensor readings.
  virtual void AddObserver(AmbientLightSensorObserver* observer);
  virtual void RemoveObserver(AmbientLightSensorObserver* observer);

  // Used by observers in their callback to get the adjustment percentage
  // suggested by the current ambient light. -1.0 is considered an error value.
  virtual double GetAmbientLightPercent() const;

  // Used by observers in their callback to get the raw reading from the sensor
  // for the ambient light level. -1 is considered an error value.
  virtual int GetAmbientLightLux() const;

  // Used by observers to get a recent log of adjustment percentages suggested
  // by the current ambient levels.  Returned values are in newest-to-oldest
  // order.
  virtual std::string DumpPercentHistory() const;

  // Used by observers to get a recent log of raw readings from the sensor.
  // Returned values are in newest-to-oldest order.
  virtual std::string DumpLuxHistory() const;

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
  double Tsl2563LuxToPercent(int luxval) const;

  // List of backlight controllers that are currently interested in updates from
  // this sensor.
  ObserverList<AmbientLightSensorObserver> observer_list_;

  // Lux value read by the class. If this read did not succeed or no read has
  // occured yet this variable is set to -1.
  int lux_value_;

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

  // History logs for the user values in oldest-to-newest order.
  std::list<double> percent_history_;
  std::list<int> lux_history_;

  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensor);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_AMBIENT_LIGHT_SENSOR_H_
