// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/external_backlight_controller.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "power_manager/powerd/policy/backlight_controller_observer.h"
#include "power_manager/powerd/system/display/display_power_setter.h"
#include "power_manager/powerd/system/display/display_watcher.h"
#include "power_manager/powerd/system/display/external_display.h"

namespace power_manager {
namespace policy {

namespace {

// Amount the brightness will be adjusted up or down in response to a user
// request, as a linearly-calculated percent in the range [0.0, 100.0].
const double kBrightnessAdjustmentPercent = 5.0;

}  // namespace

ExternalBacklightController::ExternalBacklightController()
    : display_watcher_(NULL),
      display_power_setter_(NULL),
      dimmed_for_inactivity_(false),
      off_for_inactivity_(false),
      suspended_(false),
      shutting_down_(false),
      currently_off_(false),
      num_brightness_adjustments_in_session_(0) {
}

ExternalBacklightController::~ExternalBacklightController() {
  if (display_watcher_)
    display_watcher_->RemoveObserver(this);
}

void ExternalBacklightController::Init(
    system::DisplayWatcherInterface* display_watcher,
    system::DisplayPowerSetterInterface* display_power_setter) {
  display_watcher_ = display_watcher;
  display_power_setter_ = display_power_setter;
  display_watcher_->AddObserver(this);
  UpdateDisplays(display_watcher_->GetDisplays());
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
    SessionState state) {
  if (state == SESSION_STARTED)
    num_brightness_adjustments_in_session_ = 0;
}

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
  NotifyObservers();
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
  num_brightness_adjustments_in_session_++;
  AdjustBrightnessByPercent(kBrightnessAdjustmentPercent);
  return true;
}

bool ExternalBacklightController::DecreaseUserBrightness(bool allow_off) {
  num_brightness_adjustments_in_session_++;
  AdjustBrightnessByPercent(-kBrightnessAdjustmentPercent);
  return true;
}

void ExternalBacklightController::SetDocked(bool docked) {}

int ExternalBacklightController::GetNumAmbientLightSensorAdjustments() const {
  return 0;
}

int ExternalBacklightController::GetNumUserAdjustments() const {
  return num_brightness_adjustments_in_session_;
}

void ExternalBacklightController::OnDisplaysChanged(
    const std::vector<system::DisplayInfo>& displays) {
  UpdateDisplays(displays);
}

void ExternalBacklightController::UpdateScreenPowerState() {
  bool should_turn_off = off_for_inactivity_ || suspended_ || shutting_down_;
  if (should_turn_off != currently_off_) {
    currently_off_ = should_turn_off;
    display_power_setter_->SetDisplayPower(should_turn_off ?
                                           chromeos::DISPLAY_POWER_ALL_OFF :
                                           chromeos::DISPLAY_POWER_ALL_ON,
                                           base::TimeDelta());
    NotifyObservers();
  }
}

void ExternalBacklightController::NotifyObservers() {
  FOR_EACH_OBSERVER(BacklightControllerObserver, observers_,
                    OnBrightnessChanged(currently_off_ ? 0.0 : 100.0,
                                        BRIGHTNESS_CHANGE_AUTOMATED, this));
}

void ExternalBacklightController::UpdateDisplays(
    const std::vector<system::DisplayInfo>& displays) {
  ExternalDisplayMap updated_displays;
  for (std::vector<system::DisplayInfo>::const_iterator it = displays.begin();
       it != displays.end(); ++it) {
    const system::DisplayInfo& info = *it;
    if (info.i2c_path.empty())
      continue;

    ExternalDisplayMap::const_iterator existing_display_it =
        external_displays_.find(info.drm_path);
    if (existing_display_it != external_displays_.end()) {
      // TODO(derat): Need to handle changed I2C paths?
      updated_displays[info.drm_path] = existing_display_it->second;
    } else {
      scoped_ptr<system::ExternalDisplay::RealDelegate> delegate(
          new system::ExternalDisplay::RealDelegate);
      if (!delegate->Init(info.i2c_path))
        continue;
      updated_displays[info.drm_path] = linked_ptr<system::ExternalDisplay>(
          new system::ExternalDisplay(
              delegate.PassAs<system::ExternalDisplay::Delegate>()));
    }
  }
  external_displays_.swap(updated_displays);
}

void ExternalBacklightController::AdjustBrightnessByPercent(
    double percent_offset) {
  VLOG(1) << "Adjusting brightness by " << percent_offset << "%";
  for (ExternalDisplayMap::const_iterator it = external_displays_.begin();
       it != external_displays_.end(); ++it) {
    it->second->AdjustBrightnessByPercent(percent_offset);
  }
}

}  // namespace policy
}  // namespace power_manager
