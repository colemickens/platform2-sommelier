// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/external_backlight_controller.h"

#include <algorithm>
#include <cmath>

#include "power_manager/powerd/backlight_controller_observer.h"
#include "power_manager/powerd/system/backlight_interface.h"
#include "power_manager/powerd/system/display_power_setter.h"

namespace {

// Amount that the brightness should be changed (across a [0.0, 100.0] range)
// when the brightness-increase or -decrease keys are pressed.
double kBrightnessAdjustmentPercent = 10.0;

}  // namespace

namespace power_manager {

ExternalBacklightController::ExternalBacklightController(
    system::BacklightInterface* backlight,
    system::DisplayPowerSetterInterface* display_power_setter)
    : backlight_(backlight),
      display_power_setter_(display_power_setter),
      dimmed_for_inactivity_(false),
      off_for_inactivity_(false),
      suspended_(false),
      shutting_down_(false),
      max_level_(0),
      currently_off_(false),
      num_user_adjustments_(0) {
  DCHECK(backlight_);
  DCHECK(display_power_setter_);
  backlight_->AddObserver(this);
}

ExternalBacklightController::~ExternalBacklightController() {
  backlight_->RemoveObserver(this);
}

bool ExternalBacklightController::Init() {
  // If we get restarted while Chrome is running, make sure that it doesn't get
  // wedged in a dimmed state.
  if (max_level_ <= 0)
    display_power_setter_->SetDisplaySoftwareDimming(false);

  if (!backlight_->GetMaxBrightnessLevel(&max_level_)) {
    LOG(ERROR) << "Unable to query maximum brightness level";
    max_level_ = 0;
    return false;
  }
  LOG(INFO) << "Initialized external backlight controller: "
            << "max_level=" << max_level_;
  return true;
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

void ExternalBacklightController::HandleUserActivity() {}

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
  int64 current_level = 0;
  if (!backlight_->GetCurrentBrightnessLevel(&current_level))
    return false;

  *percent = LevelToPercent(current_level);
  return true;
}

bool ExternalBacklightController::SetUserBrightnessPercent(
    double percent,
    TransitionStyle style) {
  if (max_level_ <= 0)
    return false;

  // Always perform instant transitions; there's no guarantee about how quickly
  // an external display will respond to our requests.
  if (!backlight_->SetBrightnessLevel(PercentToLevel(percent),
                                      base::TimeDelta()))
    return false;
  num_user_adjustments_++;
  FOR_EACH_OBSERVER(BacklightControllerObserver, observers_,
                    OnBrightnessChanged(percent,
                                        BRIGHTNESS_CHANGE_USER_INITIATED,
                                        this));
  return true;
}

bool ExternalBacklightController::IncreaseUserBrightness() {
  return AdjustUserBrightnessByOffset(
      kBrightnessAdjustmentPercent, true /* allow_off */);
}

bool ExternalBacklightController::DecreaseUserBrightness(bool allow_off) {
  return AdjustUserBrightnessByOffset(-kBrightnessAdjustmentPercent, allow_off);
}

void ExternalBacklightController::SetDocked(bool docked) {}

int ExternalBacklightController::GetNumAmbientLightSensorAdjustments() const {
  return 0;
}

int ExternalBacklightController::GetNumUserAdjustments() const {
  return num_user_adjustments_;
}

void ExternalBacklightController::OnBacklightDeviceChanged() {
  Init();
}

double ExternalBacklightController::LevelToPercent(int64 level) {
  if (max_level_ <= 0)
    return 0.0;

  level = std::min(std::max(level, static_cast<int64>(0)), max_level_);
  return 100.0 * level / max_level_;
}

int64 ExternalBacklightController::PercentToLevel(double percent) {
  if (max_level_ <= 0)
    return 0;

  percent = std::min(std::max(percent, 0.0), 100.0);
  return llround(percent / 100.0 * max_level_);
}

bool ExternalBacklightController::AdjustUserBrightnessByOffset(
    double percent_offset,
    bool allow_off) {
  double old_percent = 0.0;
  if (!GetBrightnessPercent(&old_percent))
    return false;

  double new_percent = old_percent + percent_offset;
  new_percent = std::min(std::max(new_percent, 0.0), 100.0);

  if (!allow_off && new_percent <= kEpsilon) {
    num_user_adjustments_++;
    return false;
  }

  return SetUserBrightnessPercent(new_percent, TRANSITION_INSTANT);
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

}  // namespace power_manager
