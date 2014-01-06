// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/external_backlight_controller.h"

#include <algorithm>
#include <cmath>

#include "power_manager/powerd/policy/backlight_controller_observer.h"
#include "power_manager/powerd/system/display_power_setter.h"

namespace power_manager {
namespace policy {

ExternalBacklightController::ExternalBacklightController()
    : display_power_setter_(NULL),
      dimmed_for_inactivity_(false),
      off_for_inactivity_(false),
      suspended_(false),
      shutting_down_(false),
      currently_off_(false) {
}

ExternalBacklightController::~ExternalBacklightController() {}

void ExternalBacklightController::Init(
    system::DisplayPowerSetterInterface* display_power_setter) {
  display_power_setter_ = display_power_setter;
}

void ExternalBacklightController::AddObserver(
    BacklightControllerObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void ExternalBacklightController::RemoveObserver(
    BacklightControllerObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void ExternalBacklightController::HandlePowerSourceChange(PowerSource source) {}

void ExternalBacklightController::HandleDisplayModeChange(DisplayMode mode) {}

void ExternalBacklightController::HandleSessionStateChange(
    SessionState state) {}

void ExternalBacklightController::HandlePowerButtonPress() {}

void ExternalBacklightController::HandleUserActivity(UserActivityType type) {}

void ExternalBacklightController::HandlePolicyChange(
    const PowerManagementPolicy& policy) {}

void ExternalBacklightController::HandleChromeStart() {
  display_power_setter_->SetDisplaySoftwareDimming(dimmed_for_inactivity_);
  display_power_setter_->SetDisplayPower(currently_off_ ?
                                         chromeos::DISPLAY_POWER_ALL_OFF :
                                         chromeos::DISPLAY_POWER_ALL_ON,
                                         base::TimeDelta());
}

void ExternalBacklightController::SetDimmedForInactivity(bool dimmed) {
  if (dimmed != dimmed_for_inactivity_) {
    dimmed_for_inactivity_ = dimmed;
    display_power_setter_->SetDisplaySoftwareDimming(dimmed);
  }
}

void ExternalBacklightController::SetOffForInactivity(bool off) {
  if (off == off_for_inactivity_)
    return;
  off_for_inactivity_ = off;
  UpdateScreenPowerState();
}

void ExternalBacklightController::SetSuspended(bool suspended) {
  if (suspended == suspended_)
    return;
  suspended_ = suspended;
  UpdateScreenPowerState();
}

void ExternalBacklightController::SetShuttingDown(bool shutting_down) {
  if (shutting_down == shutting_down_)
    return;
  shutting_down_ = shutting_down;
  UpdateScreenPowerState();
}

bool ExternalBacklightController::GetBrightnessPercent(double* percent) {
  return false;
}

bool ExternalBacklightController::SetUserBrightnessPercent(
    double percent,
    TransitionStyle style) {
  return false;
}

bool ExternalBacklightController::IncreaseUserBrightness() {
  return false;
}

bool ExternalBacklightController::DecreaseUserBrightness(bool allow_off) {
  return false;
}

void ExternalBacklightController::SetDocked(bool docked) {}

int ExternalBacklightController::GetNumAmbientLightSensorAdjustments() const {
  return 0;
}

int ExternalBacklightController::GetNumUserAdjustments() const {
  return 0;
}

void ExternalBacklightController::UpdateScreenPowerState() {
  bool should_turn_off = off_for_inactivity_ || suspended_ || shutting_down_;
  if (should_turn_off != currently_off_) {
    display_power_setter_->SetDisplayPower(should_turn_off ?
                                           chromeos::DISPLAY_POWER_ALL_OFF :
                                           chromeos::DISPLAY_POWER_ALL_ON,
                                           base::TimeDelta());
    currently_off_ = should_turn_off;
  }
}

}  // namespace policy
}  // namespace power_manager
