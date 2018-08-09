// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/cellular_controller.h"

#include "power_manager/common/prefs.h"

namespace power_manager {
namespace policy {

CellularController::CellularController() = default;

CellularController::~CellularController() = default;

void CellularController::Init(Delegate* delegate, PrefsInterface* prefs) {
  DCHECK(delegate);
  DCHECK(prefs);

  delegate_ = delegate;

  prefs->GetBool(kSetCellularTransmitPowerForProximityPref,
                 &set_transmit_power_for_proximity_);
  prefs->GetInt64(kSetCellularTransmitPowerDprGpioPref, &dpr_gpio_number_);

  if (set_transmit_power_for_proximity_)
    CHECK_GE(dpr_gpio_number_, 0) << "DPR GPIO is unspecified or invalid";
}

void CellularController::ProximitySensorDetected(UserProximity value) {
  if (set_transmit_power_for_proximity_) {
    LOG(INFO) << "Cellular power will be handled by proximity sensor";
    HandleProximityChange(value);
  }
}

void CellularController::HandleProximityChange(UserProximity proximity) {
  if (!set_transmit_power_for_proximity_)
    return;

  if (proximity_ == proximity)
    return;

  proximity_ = proximity;
  UpdateTransmitPower();
}

void CellularController::UpdateTransmitPower() {
  switch (proximity_) {
    case UserProximity::UNKNOWN:
      break;
    case UserProximity::NEAR:
      delegate_->SetCellularTransmitPower(RadioTransmitPower::LOW,
                                          dpr_gpio_number_);
      break;
    case UserProximity::FAR:
      delegate_->SetCellularTransmitPower(RadioTransmitPower::HIGH,
                                          dpr_gpio_number_);
      break;
  }
}

}  // namespace policy
}  // namespace power_manager
