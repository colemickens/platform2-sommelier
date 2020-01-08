// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/ambient_light_sensor_manager.h"

#include <memory>
#include <utility>

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

AmbientLightSensorManager::AmbientLightSensorManager() = default;

AmbientLightSensorManager::~AmbientLightSensorManager() = default;

void AmbientLightSensorManager::set_device_list_path_for_testing(
    const base::FilePath& path) {
  for (const auto& sensor : sensors_)
    sensor->set_device_list_path_for_testing(path);
}

void AmbientLightSensorManager::set_poll_interval_ms_for_testing(
    int interval_ms) {
  for (const auto& sensor : sensors_)
    sensor->set_poll_interval_ms_for_testing(interval_ms);
}

void AmbientLightSensorManager::Init(PrefsInterface* prefs) {
  prefs_ = prefs;
  int64_t num_sensors = 0;
  bool allow_ambient_eq = false;
  prefs_->GetInt64(kHasAmbientLightSensorPref, &num_sensors);
  CHECK(prefs_->GetBool(kAllowAmbientEQ, &allow_ambient_eq))
      << "Failed to read pref " << kAllowAmbientEQ;

  // Currently Ambient EQ is the only use case for color ALS. Enable color
  // support on ALS if device is allowed to have Ambient EQ feature.
  if (num_sensors == 1) {
    sensors_.push_back(
        std::make_unique<system::AmbientLightSensor>(allow_ambient_eq));
    lid_sensor_ = base_sensor_ = sensors_[0].get();
  } else if (num_sensors >= 2) {
    sensors_.push_back(std::make_unique<system::AmbientLightSensor>(
        SensorLocation::LID, allow_ambient_eq));
    sensors_.push_back(
        std::make_unique<system::AmbientLightSensor>(SensorLocation::BASE));
    lid_sensor_ = sensors_[0].get();
    base_sensor_ = sensors_[1].get();
  }
}

void AmbientLightSensorManager::Run(bool read_immediately) {
  for (const auto& sensor : sensors_)
    sensor->Init(read_immediately);
}

bool AmbientLightSensorManager::HasColorSensor() {
  for (const auto& sensor : sensors_) {
    if (sensor->IsColorSensor())
      return true;
  }
  return false;
}

AmbientLightSensorInterface*
AmbientLightSensorManager::GetSensorForInternalBacklight() {
  return lid_sensor_;
}

AmbientLightSensorInterface*
AmbientLightSensorManager::GetSensorForKeyboardBacklight() {
  return base_sensor_;
}

}  // namespace system
}  // namespace power_manager
