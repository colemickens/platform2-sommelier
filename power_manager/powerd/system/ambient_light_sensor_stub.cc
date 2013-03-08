// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/ambient_light_sensor_stub.h"

#include <string>

namespace power_manager {
namespace system {

AmbientLightSensorStub::AmbientLightSensorStub(double percent, int lux)
    : percent_(percent),
      lux_(lux) {
}

AmbientLightSensorStub::~AmbientLightSensorStub() {}

void AmbientLightSensorStub::NotifyObservers() {
  FOR_EACH_OBSERVER(AmbientLightObserver, observers_,
                    OnAmbientLightChanged(this));
}

void AmbientLightSensorStub::AddObserver(AmbientLightObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void AmbientLightSensorStub::RemoveObserver(AmbientLightObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

double AmbientLightSensorStub::GetAmbientLightPercent() { return percent_; }

int AmbientLightSensorStub::GetAmbientLightLux() { return lux_; }

std::string AmbientLightSensorStub::DumpPercentHistory() {
  return std::string();
}

std::string AmbientLightSensorStub::DumpLuxHistory() { return std::string(); }

}  // namespace system
}  // namespace power_manager
