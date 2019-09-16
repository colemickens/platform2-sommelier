// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_MANAGER_H_
#define POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_MANAGER_H_

#include <memory>
#include <vector>

#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/ambient_light_sensor.h"

namespace power_manager {
namespace system {

class AmbientLightSensorManagerInterface {
 public:
  AmbientLightSensorManagerInterface() {}
  virtual ~AmbientLightSensorManagerInterface() {}

  virtual AmbientLightSensorInterface* GetSensorForInternalBacklight() = 0;
  virtual AmbientLightSensorInterface* GetSensorForKeyboardBacklight() = 0;

  virtual bool HasColorSensor() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensorManagerInterface);
};

class AmbientLightSensorManager : public AmbientLightSensorManagerInterface {
 public:
  AmbientLightSensorManager();
  ~AmbientLightSensorManager() override;

  void set_device_list_path_for_testing(const base::FilePath& path);
  void set_poll_interval_ms_for_testing(int interval_ms);

  void SetNumSensorsAndInit(int num_sensors);
  void Run(bool read_immediately);

  bool HasColorSensor() override;

  AmbientLightSensorInterface* GetSensorForInternalBacklight() override;
  AmbientLightSensorInterface* GetSensorForKeyboardBacklight() override;

 private:
  int num_sensors_ = -1;

  // AmbientLightSensorManager object owns AmbientLightSensor object via unique
  // pointers.
  std::vector<std::unique_ptr<system::AmbientLightSensor>> sensors_;
  // Weak pointers into the relevant entries of |sensors_|.
  system::AmbientLightSensor* lid_sensor_ = nullptr;
  system::AmbientLightSensor* base_sensor_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensorManager);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_MANAGER_H_
