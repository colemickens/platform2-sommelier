// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_STUB_H_
#define POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_STUB_H_

#include "power_manager/powerd/system/ambient_light_sensor.h"

#include "base/observer_list.h"

namespace power_manager {
namespace system {

// Stub implementation of AmbientLightSensorInterface for use by tests.
class AmbientLightSensorStub : public AmbientLightSensorInterface {
 public:
  explicit AmbientLightSensorStub(int lux);
  virtual ~AmbientLightSensorStub();

  void set_lux(int lux) { lux_ = lux; }

  // Notifies |observers_| that the ambient light has changed.
  void NotifyObservers();

  // AmbientLightSensorInterface implementation:
  virtual void AddObserver(AmbientLightObserver* observer) OVERRIDE;
  virtual void RemoveObserver(AmbientLightObserver* observer) OVERRIDE;
  virtual int GetAmbientLightLux() OVERRIDE;

 private:
  ObserverList<AmbientLightObserver> observers_;

  // Value returned by GetAmbientLightLux().
  int lux_;

  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensorStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_SENSOR_STUB_H_
