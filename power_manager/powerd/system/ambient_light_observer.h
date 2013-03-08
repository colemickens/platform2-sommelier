// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_OBSERVER_H_
#define POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_OBSERVER_H_

namespace power_manager {
namespace system {

class AmbientLightSensorInterface;

// Interface for classes interested in receiving updates about the ambient
// light level from AmbientLightSensor.
class AmbientLightObserver {
 public:
  virtual ~AmbientLightObserver() {}

  // Called when the light level changes.
  virtual void OnAmbientLightChanged(AmbientLightSensorInterface* sensor) = 0;
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_AMBIENT_LIGHT_OBSERVER_H_
