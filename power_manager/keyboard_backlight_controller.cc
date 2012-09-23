// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <utility>

#include "power_manager/ambient_light_sensor.h"
#include "power_manager/backlight_interface.h"
#include "power_manager/keyboard_backlight_controller.h"
#include "power_manager/util.h"

namespace power_manager {

const double kTargetPercentDim = 20.0;
const double kTargetPercentMax = 100.0;
const double kTargetPercentMin = 0.0;

// This is how long after a video playing message is received we should wait
// until reverting to the not playing state. If another message is received in
// this interval the timeout is reset. The browser should be sending these
// messages ~5 seconds when video is playing.
const int64 kVideoTimeoutIntervalMs = 7000;

// The percentage change caused by each increment/decrement call.
const double kBrightnessPercentChangeIncrement = 10.0;

KeyboardBacklightController::KeyboardBacklightController(
    BacklightInterface* backlight,
    AmbientLightSensor* sensor)
    : is_initialized_ (false),
      backlight_(backlight),
      light_sensor_(sensor),
      observer_(NULL),
      state_(BACKLIGHT_UNINITIALIZED),
      is_video_playing_(false),
      is_fullscreen_(false),
      backlight_enabled_(true),
      max_level_(0),
      current_level_(0),
      target_percent_(0.0),
      video_timeout_timer_id_(0),
      num_als_adjustments_(0),
      num_user_adjustments_(0) {
  DCHECK(backlight) << "Cannot create KeyboardBacklightController will NULL "
                    << "backlight!";
}

KeyboardBacklightController::~KeyboardBacklightController() {
  if (light_sensor_) {
    light_sensor_->RemoveObserver(this);
    light_sensor_ = NULL;
  }
  HaltVideoTimeout();
}

bool KeyboardBacklightController::Init() {
  if (light_sensor_)
    light_sensor_->AddObserver(this);
  if (!backlight_->GetMaxBrightnessLevel(&max_level_) ||
      !backlight_->GetCurrentBrightnessLevel(&current_level_)) {
    LOG(ERROR) << "Querying backlight during initialization failed";
    is_initialized_ = false;
    return false;
  }
  target_percent_ = LevelToPercent(current_level_);
  is_initialized_ = true;
  return true;
}

void KeyboardBacklightController::SetObserver(
    BacklightControllerObserver* observer) {
  observer_ = observer;
}

double KeyboardBacklightController::GetTargetBrightnessPercent() {
  return target_percent_;
}

bool KeyboardBacklightController::GetCurrentBrightnessPercent(double* percent) {
  DCHECK(percent);
  *percent = LevelToPercent(current_level_);
  return *percent >= 0.0;
}

bool KeyboardBacklightController::SetCurrentBrightnessPercent(
    double percent,
    BrightnessChangeCause cause,
    TransitionStyle style) { // ignored
  percent = std::max(std::min(percent, kTargetPercentMax), kTargetPercentMin);
  target_percent_ = percent;
  if (!backlight_enabled_)
    return true;
  SetBrightnessLevel(PercentToLevel(percent));
  if (cause == BRIGHTNESS_CHANGE_USER_INITIATED)
    num_user_adjustments_++;
  if (observer_)
    observer_->OnBrightnessChanged(percent, cause, this);
  return true;
}

bool KeyboardBacklightController::IncreaseBrightness(
    BrightnessChangeCause cause) {
  if (state_ != BACKLIGHT_ACTIVE)
    return false;

  return SetCurrentBrightnessPercent(target_percent_
                                     + kBrightnessPercentChangeIncrement,
                                     cause,
                                     TRANSITION_INSTANT);
}

bool KeyboardBacklightController::DecreaseBrightness(
    bool allow_off,
    BrightnessChangeCause cause) {
  if (state_ != BACKLIGHT_ACTIVE)
    return false;

  return SetCurrentBrightnessPercent(target_percent_
                                     - kBrightnessPercentChangeIncrement,
                                     cause,
                                     TRANSITION_INSTANT);
}

bool KeyboardBacklightController::SetPowerState(PowerState new_state) {
  if (new_state == state_ || !is_initialized_)
    return false;

  CHECK(new_state != BACKLIGHT_UNINITIALIZED);

  PowerState old_state = state_;
  state_ = new_state;
  int64 new_level;
  switch (state_) {
    case BACKLIGHT_ACTIVE:
      new_level = PercentToLevel(target_percent_);
      break;
    case BACKLIGHT_DIM:
      new_level = PercentToLevel(kTargetPercentDim);
      new_level = new_level > current_level_ ? current_level_ : new_level;
      break;
    case BACKLIGHT_IDLE_OFF:
    case BACKLIGHT_SUSPENDED:
      new_level = PercentToLevel(kTargetPercentMin);
      break;
    default:
      new_level = current_level_;
      break;
  };

  if (new_level == current_level_ && state_ == BACKLIGHT_DIM) {
    new_state = BACKLIGHT_ALREADY_DIMMED;
  } else {
    new_level = backlight_enabled_ ? new_level : 0;
    SetBrightnessLevel(new_level);
    if (observer_) {
      observer_->OnBrightnessChanged(LevelToPercent(new_level),
                                     BRIGHTNESS_CHANGE_AUTOMATED,
                                     this);
    }
  }

  LOG(INFO) << util::PowerStateToString(old_state) << " -> "
            << util::PowerStateToString(new_state);

  return true;
}

PowerState KeyboardBacklightController::GetPowerState() const {
  return state_;
}

bool KeyboardBacklightController::IsBacklightActiveOff() {
  return (backlight_enabled_ && current_level_ == 0);
}

int KeyboardBacklightController::GetNumAmbientLightSensorAdjustments() const {
  return num_als_adjustments_;
}

int KeyboardBacklightController::GetNumUserAdjustments() const {
  return num_user_adjustments_;
}

void KeyboardBacklightController::OnBacklightDeviceChanged() {
  LOG(INFO) << "Backlight device changed; reinitializing controller";
  double target_percent = target_percent_;
  if (Init()) {
    SetCurrentBrightnessPercent(target_percent,
                                BRIGHTNESS_CHANGE_AUTOMATED,
                                TRANSITION_INSTANT);
  }
}

void KeyboardBacklightController::OnAmbientLightChanged(
    AmbientLightSensor* sensor) {
  // TODO(rharrison): Implement response behaviour to ALS input.
  NOTIMPLEMENTED();
}

void KeyboardBacklightController::OnVideoDetectorEvent(
    base::TimeTicks last_activity_time,
    bool is_fullscreen) {
  int64 timeout_interval_ms = kVideoTimeoutIntervalMs -
      (base::TimeTicks::Now() - last_activity_time).InMilliseconds();
  if (timeout_interval_ms <= 0) {
    LOG(WARNING) << "Didn't get notification about video event before timeout "
                 << "interval was over!";
    is_video_playing_ = false;
    is_fullscreen_ = false;
    return;
  }
  HaltVideoTimeout();
  is_fullscreen_ = is_fullscreen;
  is_video_playing_ = true;
  UpdateBacklightEnabled();
  video_timeout_timer_id_ = g_timeout_add(timeout_interval_ms,
                                          VideoTimeoutThunk,
                                          this);
}

void KeyboardBacklightController::UpdateBacklightEnabled() {
  bool new_backlight_enabled = (!is_video_playing_ || !is_fullscreen_);
  if (new_backlight_enabled == backlight_enabled_)
    return;

  backlight_enabled_ = new_backlight_enabled;
  double percent = backlight_enabled_ ? target_percent_ : 0;
  SetBrightnessLevel(PercentToLevel(percent));
  if (observer_) {
    observer_->OnBrightnessChanged(percent,
                                   BRIGHTNESS_CHANGE_AUTOMATED,
                                   this);
  }
}

void KeyboardBacklightController::SetBrightnessLevel(int64 new_level) {
  current_level_ = new_level;
  backlight_->SetBrightnessLevel(new_level);
}

void KeyboardBacklightController::HaltVideoTimeout() {
  if (video_timeout_timer_id_ == 0)
    return;
  g_source_remove(video_timeout_timer_id_);
  video_timeout_timer_id_ = 0;
}

gboolean KeyboardBacklightController::VideoTimeout() {
  is_video_playing_ = false;
  UpdateBacklightEnabled();
  video_timeout_timer_id_ = 0;
  return FALSE;
}

int64 KeyboardBacklightController::PercentToLevel(double percent) const {
  if (max_level_ == 0)
    return -1;
  percent = std::max(std::min(percent, 100.0), 0.0);
  return lround(static_cast<double>(max_level_) * percent / 100.0);
}

double KeyboardBacklightController::LevelToPercent(int64 level) const {
  if (max_level_ == 0)
    return -1.0;
  level = std::max(std::min(level, max_level_), static_cast<int64>(0));
  return static_cast<double>(level) * 100.0 / max_level_;
}

}  // namespace power_manager
