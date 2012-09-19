// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/external_backlight_controller.h"

#include <dbus/dbus-glib-lowlevel.h>

#include <algorithm>
#include <cmath>

#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/backlight_interface.h"
#include "power_manager/powerd/monitor_reconfigure.h"

namespace {

// Amount that the brightness should be changed (across a [0.0, 100.0] range)
// when the brightness-increase or -decrease keys are pressed.
double kBrightnessAdjustmentPercent = 10.0;

}  // namespace

namespace power_manager {

ExternalBacklightController::ExternalBacklightController(
    BacklightInterface* backlight)
    : backlight_(backlight),
      monitor_reconfigure_(NULL),
      observer_(NULL),
      power_state_(BACKLIGHT_UNINITIALIZED),
      max_level_(0),
      currently_dimming_(false),
      currently_off_(false),
      num_user_adjustments_(0),
      disable_dbus_for_testing_(false) {
  backlight_->set_observer(this);
}

ExternalBacklightController::~ExternalBacklightController() {
  backlight_->set_observer(NULL);
}

bool ExternalBacklightController::Init() {
  // If we get restarted while Chrome is running, make sure that it doesn't get
  // wedged in a dimmed state.
  if (!disable_dbus_for_testing_) {
    SendSoftwareDimmingSignal(currently_dimming_ ?
                              kSoftwareScreenDimmingIdle :
                              kSoftwareScreenDimmingNone);
  }

  if (!backlight_->GetMaxBrightnessLevel(&max_level_)) {
    LOG(ERROR) << "Unable to query maximum brightness level";
    max_level_ = 0;
    return false;
  }
  LOG(INFO) << "Initialized external backlight controller: "
            << "max_level=" << max_level_;
  return true;
}

void ExternalBacklightController::SetMonitorReconfigure(
    MonitorReconfigure* monitor_reconfigure) {
  monitor_reconfigure_ = monitor_reconfigure;
}

void ExternalBacklightController::SetObserver(
    BacklightControllerObserver* observer) {
  observer_ = observer;
}

double ExternalBacklightController::GetTargetBrightnessPercent() {
  // Just query and return the current level.  The external display could've
  // been set to a different level without us hearing about it.
  double current_percent = 0.0;
  if (!GetCurrentBrightnessPercent(&current_percent))
    return 100.0;

  return current_percent;
}

bool ExternalBacklightController::GetCurrentBrightnessPercent(double* percent) {
  int64 current_level = 0;
  if (!backlight_->GetCurrentBrightnessLevel(&current_level))
    return false;

  *percent = LevelToPercent(current_level);
  return true;
}

bool ExternalBacklightController::SetCurrentBrightnessPercent(
    double percent,
    BrightnessChangeCause cause,
    TransitionStyle style) {
  if (max_level_ <= 0)
    return false;
  // Always perform instant transitions; there's no guarantee about how quickly
  // an external display will respond to our requests.
  if (!backlight_->SetBrightnessLevel(PercentToLevel(percent)))
    return false;
  if (cause == BRIGHTNESS_CHANGE_USER_INITIATED)
    num_user_adjustments_++;
  if (observer_)
    observer_->OnBrightnessChanged(GetTargetBrightnessPercent(), cause, this);
  return true;
}

bool ExternalBacklightController::IncreaseBrightness(
    BrightnessChangeCause cause) {
  return AdjustBrightnessByOffset(kBrightnessAdjustmentPercent, cause);
}

bool ExternalBacklightController::DecreaseBrightness(
    bool allow_off,
    BrightnessChangeCause cause) {
  return AdjustBrightnessByOffset(-kBrightnessAdjustmentPercent, cause);
}

bool ExternalBacklightController::SetPowerState(PowerState state) {
  DCHECK(state != BACKLIGHT_UNINITIALIZED);
  power_state_ = state;

  bool should_dim = state != BACKLIGHT_ACTIVE;
  if (should_dim != currently_dimming_) {
    if (!disable_dbus_for_testing_) {
      SendSoftwareDimmingSignal(should_dim ?
                                kSoftwareScreenDimmingIdle :
                                kSoftwareScreenDimmingNone);
    }
    currently_dimming_ = should_dim;
  }

  bool should_turn_off =
      state == BACKLIGHT_IDLE_OFF || state == BACKLIGHT_SUSPENDED;
  if (should_turn_off != currently_off_) {
    if (monitor_reconfigure_) {
      if (should_turn_off )
        monitor_reconfigure_->SetScreenOff();
      else
        monitor_reconfigure_->SetScreenOn();
    }
    currently_off_ = should_turn_off;
  }

  return true;
}

PowerState ExternalBacklightController::GetPowerState() const {
  return power_state_;
}

bool ExternalBacklightController::OnPlugEvent(bool is_plugged) {
  return false;
}

bool ExternalBacklightController::IsBacklightActiveOff() {
  return false;
}

int ExternalBacklightController::GetNumAmbientLightSensorAdjustments() const {
  return 0;
}

int ExternalBacklightController::GetNumUserAdjustments() const {
  return num_user_adjustments_;
}

void ExternalBacklightController::OnBacklightDeviceChanged() {
  Init();
}

void ExternalBacklightController::OnAmbientLightChanged(
    AmbientLightSensor* sensor) {
  // Desktop systems are not expected to have an ALS
  NOTIMPLEMENTED();
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

bool ExternalBacklightController::AdjustBrightnessByOffset(
    double percent_offset,
    BrightnessChangeCause cause) {
  double percent = 0.0;
  if (!GetCurrentBrightnessPercent(&percent))
    return false;

  percent += percent_offset;
  percent = std::min(std::max(percent, 0.0), 100.0);
  return SetCurrentBrightnessPercent(percent, cause, TRANSITION_INSTANT);
}

void ExternalBacklightController::SendSoftwareDimmingSignal(int state) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(
      kPowerManagerServicePath,
      kPowerManagerInterface,
      kSoftwareScreenDimmingRequestedSignal);
  CHECK(signal);

  dbus_message_append_args(signal,
                           DBUS_TYPE_INT32, &state,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

}  // namespace power_manager
