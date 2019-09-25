// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_H_
#define POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_H_

#include <list>
#include <string>

#include <base/compiler_specific.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/observer_list.h>
#include <base/timer/timer.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/ambient_light_observer.h"
#include "power_manager/powerd/system/async_file_reader.h"

namespace power_manager {
namespace system {

class AmbientLightSensorInterface {
 public:
  AmbientLightSensorInterface() {}
  virtual ~AmbientLightSensorInterface() {}

  // Adds or removes observers for sensor readings.
  virtual void AddObserver(AmbientLightObserver* observer) = 0;
  virtual void RemoveObserver(AmbientLightObserver* observer) = 0;

  // Whether or not this ALS supports color readings.
  virtual bool IsColorSensor() const = 0;

  // Used by observers in their callback to get the raw reading from the sensor
  // for the ambient light level. -1 is considered an error value.
  virtual int GetAmbientLightLux() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensorInterface);
};

class AmbientLightSensor : public AmbientLightSensorInterface {
 public:
  // Number of failed init attempts before AmbientLightSensor will start logging
  // warnings or stop trying entirely.
  static const int kNumInitAttemptsBeforeLogging;
  static const int kNumInitAttemptsBeforeGivingUp;

  AmbientLightSensor();
  ~AmbientLightSensor() override;

  void set_device_list_path_for_testing(const base::FilePath& path) {
    device_list_path_ = path;
  }
  void set_poll_interval_ms_for_testing(int interval_ms) {
    poll_interval_ms_ = interval_ms;
  }

  // Starts polling. If |read_immediately| is true, ReadAls() will also
  // immediately be called synchronously. This is separate from c'tor so that
  // tests can call set_*_for_testing() first.
  void Init(bool read_immediately);

  // Returns the path to the illuminance file being monitored, or an empty path
  // if a device has not yet been found.
  base::FilePath GetIlluminancePath() const;

  // If |poll_timer_| is running, calls ReadAls() and returns true. Otherwise,
  // returns false.
  bool TriggerPollTimerForTesting();

  // AmbientLightSensorInterface implementation:
  void AddObserver(AmbientLightObserver* observer) override;
  void RemoveObserver(AmbientLightObserver* observer) override;
  bool IsColorSensor() const override;
  int GetAmbientLightLux() override;

 private:
  // Starts |poll_timer_|.
  void StartTimer();

  // Handler for a periodic event that reads the ambient light sensor.
  void ReadAls();

  // Asynchronous I/O success and error handlers, respectively.
  void ReadCallback(const std::string& data);
  void ErrorCallback();

  // Initializes |als_file_|. Returns true on success.
  bool InitAlsFile();

  // Path containing backlight devices.  Typically under /sys, but can be
  // overridden by tests.
  base::FilePath device_list_path_;

  // Runs ReadAls().
  base::RepeatingTimer poll_timer_;

  // Time between polls of the sensor file, in milliseconds.
  int poll_interval_ms_;

  // List of backlight controllers that are currently interested in updates from
  // this sensor.
  base::ObserverList<AmbientLightObserver> observers_;

  // Lux value read by the class. If this read did not succeed or no read has
  // occured yet this variable is set to -1.
  int lux_value_;

  // Number of attempts to find and open the lux file made so far.
  int num_init_attempts_;

  // This is the ambient light sensor asynchronous file I/O object.
  AsyncFileReader als_file_;

  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensor);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_H_
