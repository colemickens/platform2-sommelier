// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_STUB_H_
#define POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_STUB_H_

#include <base/observer_list.h>

#include "power_manager/powerd/system/ambient_light_sensor.h"

namespace power_manager {
namespace system {

// Stub implementation of AmbientLightSensorInterface for use by tests.
class AmbientLightSensorStub : public AmbientLightSensorInterface {
 public:
  explicit AmbientLightSensorStub(int lux);
  ~AmbientLightSensorStub() override;

  void set_lux(int lux) { lux_ = lux; }

  // Notifies |observers_| that the ambient light has changed.
  void NotifyObservers();

  // AmbientLightSensorInterface implementation:
  void AddObserver(AmbientLightObserver* observer) override;
  void RemoveObserver(AmbientLightObserver* observer) override;
  bool IsColorSensor() const override;
  int GetAmbientLightLux() override;

 private:
  base::ObserverList<AmbientLightObserver> observers_;

  // Value returned by GetAmbientLightLux().
  int lux_;

  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensorStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_STUB_H_
