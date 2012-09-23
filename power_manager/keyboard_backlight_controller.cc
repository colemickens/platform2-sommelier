// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "power_manager/ambient_light_sensor.h"
#include "power_manager/backlight_interface.h"
#include "power_manager/keyboard_backlight_controller.h"
#include "power_manager/util.h"

namespace {

// Default control values for the target percent.
const double kTargetPercentDim = 10.0;
const double kTargetPercentMax = 100.0;
const double kTargetPercentMin = 0.0;

// Number of light sensor responses required to overcome temporal hysteresis.
const int kAlsHystResponse = 2;

// This is how long after a video playing message is received we should wait
// until reverting to the not playing state. If another message is received in
// this interval the timeout is reset. The browser should be sending these
// messages ~5 seconds when video is playing.
const int64 kVideoTimeoutIntervalMs = 7000;

}  // namespace

namespace power_manager {

KeyboardBacklightController::KeyboardBacklightController(
    BacklightInterface* backlight,
    PowerPrefsInterface* prefs,
    AmbientLightSensor* sensor)
    : is_initialized_ (false),
      backlight_(backlight),
      prefs_(prefs),
      light_sensor_(sensor),
      observer_(NULL),
      state_(BACKLIGHT_UNINITIALIZED),
      is_video_playing_(false),
      is_fullscreen_(false),
      user_enabled_(true),
      video_enabled_(true),
      max_level_(0),
      current_level_(0),
      target_percent_(0.0),
      target_percent_dim_(kTargetPercentDim),
      target_percent_max_(kTargetPercentMax),
      target_percent_min_(kTargetPercentMin),
      hysteresis_state_(ALS_HYST_IDLE),
      hysteresis_count_(0),
      current_step_index_(0),
      video_timeout_timer_id_(0),
      num_als_adjustments_(0),
      num_user_adjustments_(0) {
  DCHECK(backlight) << "Cannot create KeyboardBacklightController will NULL "
                    << "backlight!";
  if (light_sensor_)
    light_sensor_->AddObserver(this);
}

KeyboardBacklightController::~KeyboardBacklightController() {
  if (light_sensor_) {
    light_sensor_->RemoveObserver(this);
    light_sensor_ = NULL;
  }
  HaltVideoTimeout();
}

bool KeyboardBacklightController::Init() {
  if (!backlight_->GetMaxBrightnessLevel(&max_level_) ||
      !backlight_->GetCurrentBrightnessLevel(&current_level_)) {
    LOG(ERROR) << "Querying backlight during initialization failed";
    is_initialized_ = false;
    return false;
  }

  ReadPrefs();

  // This needs to be clamped since the brightness steps that are defined might
  // not use the whole range of the backlight, so the EC set level might be out
  // of range.
  double percent = std::min(LevelToPercent(current_level_),
                            target_percent_max_);
  // Select the nearest step to the current backlight level and adjust the
  // target percent in line with it. Set |percent_delta| to an arbitrary too
  // large delta and go through |brightness_steps_| minimizing the difference
  // between the step's |target_percent_| and the percent calculated above.
  double percent_delta = 2 * target_percent_max_;
  for (size_t i = 0; i < brightness_steps_.size(); i++) {
    double temp_delta = abs(percent - brightness_steps_[i].target_percent);
    if (temp_delta < percent_delta) {
      percent_delta = temp_delta;
      current_step_index_ = i;
    }
  }
  CHECK(percent_delta < 2 * target_percent_max_);
  SetCurrentBrightnessPercent(
      brightness_steps_[current_step_index_].target_percent,
      BRIGHTNESS_CHANGE_AUTOMATED,
      TRANSITION_INSTANT);

  // Create a synthetic lux value that is inline with |current_step_index_|.
  lux_level_ = brightness_steps_[current_step_index_].decrease_threshold +
      (brightness_steps_[current_step_index_].increase_threshold -
       brightness_steps_[current_step_index_].decrease_threshold) / 2;
  LOG(INFO) << "Created synthetic lux value of " << lux_level_;

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
  if (cause == BRIGHTNESS_CHANGE_USER_INITIATED)
    num_user_adjustments_++;
  target_percent_ = std::max(std::min(percent, target_percent_max_),
                             target_percent_min_);
  int64 new_level;
  // Detemine if the target percent should be used or if the state overrides
  // that.
  switch (state_) {
    case BACKLIGHT_ACTIVE:
      new_level = PercentToLevel(target_percent_);
      break;
    case BACKLIGHT_DIM:
      new_level = PercentToLevel(target_percent_dim_);
      new_level = std::min(new_level, current_level_);
      break;
    case BACKLIGHT_IDLE_OFF:
    case BACKLIGHT_SUSPENDED:
      new_level = PercentToLevel(target_percent_min_);
      break;
    default:
      new_level = current_level_;
      break;
  };
  if (!user_enabled_ || !video_enabled_)
    new_level = target_percent_min_;
  if (new_level != current_level_) {
    WriteBrightnessLevel(new_level);
    if (observer_) {
      observer_->OnBrightnessChanged(LevelToPercent(new_level),
                                     cause,
                                     this);
    }
    return true;
  }
  return false;
}

bool KeyboardBacklightController::IncreaseBrightness(
    BrightnessChangeCause cause) {
  if (!user_enabled_) {
    user_enabled_ = true;
    SetCurrentBrightnessPercent(target_percent_,
                                cause,
                                TRANSITION_INSTANT);
    return true;
  }
  return false;
}

bool KeyboardBacklightController::DecreaseBrightness(
    bool allow_off,
    BrightnessChangeCause cause) {
  if (cause != BRIGHTNESS_CHANGE_USER_INITIATED)
    LOG(WARNING) << "Received non-user DecreaseBrightness call, ignoring!";
  if (user_enabled_) {
    user_enabled_ = false;
    SetCurrentBrightnessPercent(target_percent_,
                                cause,
                                TRANSITION_INSTANT);
    return true;
  }
  return false;
}

bool KeyboardBacklightController::SetPowerState(PowerState new_state) {
  if (new_state == state_ || !is_initialized_)
    return false;
  CHECK(new_state != BACKLIGHT_UNINITIALIZED);
  LOG(INFO) << util::PowerStateToString(state_) << " -> "
            << util::PowerStateToString(new_state);
  state_ = new_state;
  if (current_level_ > PercentToLevel(target_percent_dim_) ||
      state_ != BACKLIGHT_DIM) {
    SetCurrentBrightnessPercent(target_percent_,
                                BRIGHTNESS_CHANGE_AUTOMATED,
                                TRANSITION_INSTANT);
  }
  return true;
}

PowerState KeyboardBacklightController::GetPowerState() const {
  return state_;
}

bool KeyboardBacklightController::IsBacklightActiveOff() {
  return (video_enabled_ && current_level_ == 0);
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
#ifndef HAS_ALS
  LOG(WARNING) << "Got ALS reading from platform supposed to have no ALS. "
               << "Please check the platform ALS configuration.";
#endif

  if (light_sensor_ != sensor) {
    LOG(WARNING) << "Received AmbientLightChange from unknown sensor";
    return;
  }

  int new_lux = light_sensor_->GetAmbientLightLux();
  if (new_lux < 0) {
    LOG(WARNING) << "ALS doesn't have valid value after sending "
                 << "OnAmbientLightChanged";
    return;
  }

  if (new_lux == lux_level_) {
    hysteresis_state_ = ALS_HYST_IDLE;
    return;
  }

  ssize_t i = -1;
  const ssize_t num_steps = brightness_steps_.size();
  if (new_lux > lux_level_) {  // Brightness Increasing
    if (hysteresis_state_ != ALS_HYST_UP) {
      hysteresis_state_ = ALS_HYST_UP;
      hysteresis_count_ = 0;
    }
    for (i = current_step_index_; i < num_steps; i++) {
      if (new_lux < brightness_steps_[i].increase_threshold ||
          brightness_steps_[i].increase_threshold == -1)
        break;
    }
  } else {  // Brightness decreasing
    if (hysteresis_state_ != ALS_HYST_DOWN) {
      hysteresis_state_ = ALS_HYST_DOWN;
      hysteresis_count_ = 0;
    }
    for (i = current_step_index_; i >= 0; i--) {
      if (new_lux > brightness_steps_[i].decrease_threshold ||
          brightness_steps_[i].decrease_threshold == -1)
        break;
    }
  }

  if (i >= num_steps || i < 0) {
    LOG(ERROR) << "When trying to find new brightness step for lux value of "
               << new_lux << ", index ended up out of range with a value of "
               << i;
    return;
  }

  if (current_step_index_ == i)
    return;

  hysteresis_count_++;
  if (hysteresis_count_ >= kAlsHystResponse) {
    current_step_index_ = i;
    lux_level_ = new_lux;
    hysteresis_count_ = 1;
    SetCurrentBrightnessPercent(
        brightness_steps_[current_step_index_].target_percent,
        BRIGHTNESS_CHANGE_AUTOMATED,
        TRANSITION_INSTANT);
  }
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

void KeyboardBacklightController::ReadPrefs() {
  if (!prefs_->GetDouble(kKeyboardBacklightDimPercentPref,
                         &target_percent_dim_))
    target_percent_dim_ = kTargetPercentDim;
  if (!prefs_->GetDouble(kKeyboardBacklightMaxPercentPref,
                         &target_percent_max_))
    target_percent_max_ = kTargetPercentMax;
  if (!prefs_->GetDouble(kKeyboardBacklightMinPercentPref,
                         &target_percent_min_))
    target_percent_min_ = kTargetPercentMin;

  std::string steps_input_str;
  if (prefs_->GetString(kKeyboardBacklightStepsPref, &steps_input_str)) {
    std::vector<std::string> steps_inputs;
    base::SplitString(steps_input_str, '\n', &steps_inputs);
    for (std::vector<std::string>::iterator iter = steps_inputs.begin();
         iter < steps_inputs.end(); ++iter) {
      std::vector<std::string> step_segs;
      base::SplitString(*iter, ' ', &step_segs);
      if (step_segs.size() != 3) {
        LOG(ERROR) << "Skipping line in keyboard brightness steps file: "
                   << *iter;
        continue;
      }
      struct BrightnessStep new_step;
      if (!base::StringToDouble(step_segs[0], &new_step.target_percent) ||
          !base::StringToInt(step_segs[1], &new_step.decrease_threshold) ||
          !base::StringToInt(step_segs[2],  &new_step.increase_threshold)) {
        LOG(ERROR) << "Failure in parse string: " << *iter << " -> ("
                   << new_step.target_percent << ", "
                   << new_step.decrease_threshold << ", "
                   << new_step.increase_threshold << ")";
      } else {
        brightness_steps_.push_back(new_step);
      }
    }
  }

  // If don't have any values in |brightness_steps_| insert a default value so
  // that we can effectively ignore the ALS, but still operate in a reasonable
  // manner.
  if (brightness_steps_.empty()) {
    struct BrightnessStep default_step;
    default_step.target_percent = target_percent_max_;
    default_step.decrease_threshold = -1;
    default_step.increase_threshold = -1;
    brightness_steps_.push_back(default_step);
    LOG(INFO) << "No brightness steps read; inserted default step = ("
              << default_step.target_percent << ", "
              << default_step.decrease_threshold << ", "
              << default_step.increase_threshold << ")";
  }
}

void KeyboardBacklightController::UpdateBacklightEnabled() {
  bool new_video_enabled = (!is_video_playing_ || !is_fullscreen_);
  if (new_video_enabled == video_enabled_)
    return;

  video_enabled_ = new_video_enabled;
  SetCurrentBrightnessPercent(target_percent_,
                              BRIGHTNESS_CHANGE_AUTOMATED,
                              TRANSITION_INSTANT);
}

void KeyboardBacklightController::WriteBrightnessLevel(int64 new_level) {
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
