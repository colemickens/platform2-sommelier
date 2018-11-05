// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/cellular_controller.h"

#include <algorithm>

#include "power_manager/common/prefs.h"

namespace power_manager {
namespace policy {

CellularController::CellularController() = default;

CellularController::~CellularController() = default;

void CellularController::Init(Delegate* delegate, PrefsInterface* prefs) {
  DCHECK(delegate);
  DCHECK(prefs);

  delegate_ = delegate;

  prefs->GetBool(kSetCellularTransmitPowerForTabletModePref,
                 &set_transmit_power_for_tablet_mode_);
  prefs->GetBool(kSetCellularTransmitPowerForProximityPref,
                 &set_transmit_power_for_proximity_);
  prefs->GetInt64(kSetCellularTransmitPowerDprGpioPref, &dpr_gpio_number_);

  if (set_transmit_power_for_proximity_ || set_transmit_power_for_tablet_mode_)
    CHECK_GE(dpr_gpio_number_, 0) << "DPR GPIO is unspecified or invalid";
}

void CellularController::ProximitySensorDetected(UserProximity value) {
  if (set_transmit_power_for_proximity_) {
    if (set_transmit_power_for_tablet_mode_) {
      LOG(INFO) << "Cellular power will be handled by proximity sensor and "
                   "tablet mode";
    } else {
      LOG(INFO) << "Cellular power will be handled by proximity sensor";
    }
    HandleProximityChange(value);
  }
}

void CellularController::HandleTabletModeChange(TabletMode mode) {
  if (!set_transmit_power_for_tablet_mode_)
    return;

  if (tablet_mode_ == mode)
    return;

  tablet_mode_ = mode;
  UpdateTransmitPower();
}

void CellularController::HandleProximityChange(UserProximity proximity) {
  if (!set_transmit_power_for_proximity_)
    return;

  if (proximity_ == proximity)
    return;

  proximity_ = proximity;
  UpdateTransmitPower();
}

RadioTransmitPower CellularController::DetermineTransmitPower() const {
  RadioTransmitPower proximity_power = RadioTransmitPower::HIGH;
  RadioTransmitPower tablet_mode_power = RadioTransmitPower::HIGH;

  if (set_transmit_power_for_proximity_) {
    switch (proximity_) {
      case UserProximity::UNKNOWN:
        break;
      case UserProximity::NEAR:
        proximity_power = RadioTransmitPower::LOW;
        break;
      case UserProximity::FAR:
        proximity_power = RadioTransmitPower::HIGH;
        break;
    }
  }

  if (set_transmit_power_for_tablet_mode_) {
    switch (tablet_mode_) {
      case TabletMode::UNSUPPORTED:
        break;
      case TabletMode::ON:
        tablet_mode_power = RadioTransmitPower::LOW;
        break;
      case TabletMode::OFF:
        tablet_mode_power = RadioTransmitPower::HIGH;
        break;
    }
  }

  if (proximity_power == RadioTransmitPower::LOW) {
    return RadioTransmitPower::LOW;
  }
  if (tablet_mode_power == RadioTransmitPower::LOW) {
    return RadioTransmitPower::LOW;
  }
  return RadioTransmitPower::HIGH;
}

void CellularController::UpdateTransmitPower() {
  RadioTransmitPower wanted_power = DetermineTransmitPower();
  delegate_->SetCellularTransmitPower(wanted_power, dpr_gpio_number_);
}

}  // namespace policy
}  // namespace power_manager
