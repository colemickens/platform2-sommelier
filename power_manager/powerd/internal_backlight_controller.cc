// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/internal_backlight_controller.h"

#include <sys/time.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/powerd/backlight_controller_observer.h"
#include "power_manager/powerd/system/ambient_light_sensor.h"
#include "power_manager/powerd/system/display_power_setter.h"

namespace power_manager {

namespace {

// Minimum and maximum valid values for percentages.
const double kMinPercent = 0.0;
const double kMaxPercent = 100.0;

// When going into the idle-induced dim state, the backlight dims to this
// fraction (in the range [0.0, 1.0]) of its maximum brightness level.  This is
// a fraction rather than a percent so it won't change if
// kDefaultLevelToPercentExponent is modified.
const double kDimmedBrightnessFraction = 0.1;

// Minimum brightness, as a fraction of the maximum level in the range [0.0,
// 1.0], that we'll remain at before turning the backlight off entirely.  This
// is arbitrarily chosen but seems to be a reasonable marginally-visible
// brightness for a darkened room on current devices: http://crosbug.com/24569.
// A higher level can be set via the kMinVisibleBacklightLevelPref setting.
// This is a fraction rather than a percent so it won't change if
// kDefaultLevelToPercentExponent is modified.
const double kDefaultMinVisibleBrightnessFraction = 0.0065;

// Number of light sensor responses required to overcome temporal hysteresis.
const int kAlsHystResponse = 2;

// Backlight change (in %) required to overcome light sensor level hysteresis.
const int kAlsHystPercent = 3;

// Value for |level_to_percent_exponent_|, assuming that at least
// |kMinLevelsForNonLinearScale| brightness levels are available -- if not, we
// just use 1.0 to give us a linear scale.
const double kDefaultLevelToPercentExponent = 0.5;

// Minimum number of brightness levels needed before we use a non-linear mapping
// between levels and percents.
const double kMinLevelsForNonLinearMapping = 100;

// Returns the animation duration for |transition|.
base::TimeDelta TransitionStyleToTimeDelta(
    BacklightController::TransitionStyle transition) {
  switch (transition) {
    case BacklightController::TRANSITION_INSTANT:
      return base::TimeDelta();
    case BacklightController::TRANSITION_FAST:
      return base::TimeDelta::FromMilliseconds(kFastBacklightTransitionMs);
    case BacklightController::TRANSITION_SLOW:
      return base::TimeDelta::FromMilliseconds(kSlowBacklightTransitionMs);
  }
  NOTREACHED();
  return base::TimeDelta();
}

// Clamps |percent| to fit between kMinVisiblePercent and 100.
double ClampPercentToVisibleRange(double percent) {
  return std::min(kMaxPercent,
      std::max(InternalBacklightController::kMinVisiblePercent, percent));
}

}  // namespace

const int64 InternalBacklightController::kMaxBrightnessSteps = 16;
const double InternalBacklightController::kMinVisiblePercent =
    kMaxPercent / kMaxBrightnessSteps;

InternalBacklightController::InternalBacklightController(
    system::BacklightInterface* backlight,
    PrefsInterface* prefs,
    system::AmbientLightSensorInterface* sensor,
    system::DisplayPowerSetterInterface* display_power_setter)
    : backlight_(backlight),
      prefs_(prefs),
      light_sensor_(sensor),
      display_power_setter_(display_power_setter),
      power_source_(POWER_BATTERY),
      display_mode_(DISPLAY_NORMAL),
      dimmed_for_inactivity_(false),
      off_for_inactivity_(false),
      suspended_(false),
      shutting_down_(false),
      has_seen_als_event_(false),
      has_seen_power_source_change_(false),
      als_offset_percent_(0.0),
      als_hysteresis_percent_(0.0),
      als_temporal_state_(ALS_HYST_IMMEDIATE),
      als_adjustment_count_(0),
      user_adjustment_count_(0),
      plugged_offset_percent_(0.0),
      unplugged_offset_percent_(0.0),
      user_requested_zero_(false),
      max_level_(0),
      min_visible_level_(0),
      instant_transitions_below_min_level_(false),
      ignore_ambient_light_(false),
      step_percent_(1.0),
      dimmed_brightness_percent_(kDimmedBrightnessFraction * 100.0),
      level_to_percent_exponent_(kDefaultLevelToPercentExponent),
      current_level_(0),
      display_power_state_(chromeos::DISPLAY_POWER_ALL_ON) {
  DCHECK(backlight_);
  DCHECK(prefs_);
  DCHECK(display_power_setter_);
  if (light_sensor_)
    light_sensor_->AddObserver(this);
}

InternalBacklightController::~InternalBacklightController() {
  if (light_sensor_) {
    light_sensor_->RemoveObserver(this);
    light_sensor_ = NULL;
  }
}

bool InternalBacklightController::Init() {
  if (!backlight_->GetMaxBrightnessLevel(&max_level_) ||
      !backlight_->GetCurrentBrightnessLevel(&current_level_)) {
    LOG(ERROR) << "Querying backlight during initialization failed";
    return false;
  }

  ReadPrefs();

  if (max_level_ == min_visible_level_ || kMaxBrightnessSteps == 1) {
    step_percent_ = 100.0;
  } else {
    // 1 is subtracted from kMaxBrightnessSteps to account for the step between
    // |min_brightness_level_| and 0.
    step_percent_ =
        (kMaxPercent - kMinVisiblePercent) /
        std::min(kMaxBrightnessSteps - 1, max_level_ - min_visible_level_);
  }
  CHECK_GT(step_percent_, 0.0);

  level_to_percent_exponent_ =
      max_level_ >= kMinLevelsForNonLinearMapping ?
          kDefaultLevelToPercentExponent :
          1.0;

  dimmed_brightness_percent_ = ClampPercentToVisibleRange(
      LevelToPercent(lround(kDimmedBrightnessFraction * max_level_)));

  // Ensure that the screen isn't stuck in an off state if powerd got
  // restarted for some reason.
  SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON, base::TimeDelta());

  LOG(INFO) << "Backlight has range [0, " << max_level_ << "] with "
            << step_percent_ << "% step and minimum-visible level of "
            << min_visible_level_ << "; current level is " << current_level_
            << " (" << LevelToPercent(current_level_) << "%)";
  return true;
}

double InternalBacklightController::LevelToPercent(int64 raw_level) {
  // If the passed-in level is below the minimum visible level, just map it
  // linearly into [0, kMinVisiblePercent).
  if (raw_level < min_visible_level_)
    return kMinVisiblePercent * raw_level / min_visible_level_;

  // Since we're at or above the minimum level, we know that we're at 100% if
  // the min and max are equal.
  if (min_visible_level_ == max_level_)
    return 100.0;

  double linear_fraction =
      static_cast<double>(raw_level - min_visible_level_) /
      (max_level_ - min_visible_level_);
  return kMinVisiblePercent + (kMaxPercent - kMinVisiblePercent) *
      pow(linear_fraction, level_to_percent_exponent_);
}

int64 InternalBacklightController::PercentToLevel(double percent) {
  if (percent < kMinVisiblePercent)
    return lround(min_visible_level_ * percent / kMinVisiblePercent);

  if (percent == kMaxPercent)
    return max_level_;

  double linear_fraction = (percent - kMinVisiblePercent) /
                           (kMaxPercent - kMinVisiblePercent);
  return lround(min_visible_level_ + (max_level_ - min_visible_level_) *
      pow(linear_fraction, 1.0 / level_to_percent_exponent_));
}

void InternalBacklightController::AddObserver(
    BacklightControllerObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void InternalBacklightController::RemoveObserver(
    BacklightControllerObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void InternalBacklightController::HandlePowerSourceChange(PowerSource source) {
  if (has_seen_power_source_change_ && power_source_ == source)
    return;

  VLOG(2) << "Power source changed to " << PowerSourceToString(source);

  // Ensure that the screen isn't dimmed in response to a transition to AC
  // or brightened in response to a transition to battery.
  if (has_seen_power_source_change_) {
    bool plugged = source == POWER_AC;
    if (plugged && unplugged_offset_percent_ > plugged_offset_percent_)
      plugged_offset_percent_ = unplugged_offset_percent_;
    else if (!plugged && unplugged_offset_percent_ > plugged_offset_percent_)
      unplugged_offset_percent_ = plugged_offset_percent_;
  }

  power_source_ = source;
  has_seen_power_source_change_ = true;
  UpdateState();
}

void InternalBacklightController::HandleDisplayModeChange(DisplayMode mode) {
  if (display_mode_ == mode)
    return;

  display_mode_ = mode;

  // If there's no external display now, make sure that the panel is on.
  if (display_mode_ == DISPLAY_NORMAL)
    EnsureUserBrightnessIsNonzero();
}

void InternalBacklightController::HandleSessionStateChange(SessionState state) {
  EnsureUserBrightnessIsNonzero();
}

void InternalBacklightController::HandlePowerButtonPress() {
  EnsureUserBrightnessIsNonzero();
}

void InternalBacklightController::SetDimmedForInactivity(bool dimmed) {
  if (dimmed_for_inactivity_ == dimmed)
    return;

  VLOG(2) << (dimmed ? "Dimming" : "No longer dimming") << " for inactivity";
  dimmed_for_inactivity_ = dimmed;
  UpdateState();
}

void InternalBacklightController::SetOffForInactivity(bool off) {
  if (off_for_inactivity_ == off)
    return;

  VLOG(2) << (off ? "Turning backlight off" : "No longer keeping backlight off")
          << " for inactivity";
  off_for_inactivity_ = off;
  UpdateState();
}

void InternalBacklightController::SetSuspended(bool suspended) {
  if (suspended_ == suspended)
    return;

  VLOG(2) << (suspended ? "Suspending" : "Unsuspending") << " backlight";
  suspended_ = suspended;
  UpdateState();
}

void InternalBacklightController::SetShuttingDown(bool shutting_down) {
  if (shutting_down_ == shutting_down)
    return;

  if (shutting_down)
    VLOG(2) << "Preparing backlight for shutdown";
  else
    LOG(WARNING) << "Exiting shutting-down state";
  shutting_down_ = shutting_down;
  UpdateState();
}

bool InternalBacklightController::GetBrightnessPercent(double* percent) {
  DCHECK(percent);
  *percent = LevelToPercent(current_level_);
  return true;
}

bool InternalBacklightController::SetUserBrightnessPercent(
    double percent,
    TransitionStyle style) {
  VLOG(1) << "Got user-triggered request to set brightness to "
          << percent << "%";
  user_adjustment_count_++;
  user_requested_zero_ = percent <= kEpsilon;
  return SetUndimmedBrightnessPercent(percent, style,
                                      BRIGHTNESS_CHANGE_USER_INITIATED);
}

bool InternalBacklightController::IncreaseUserBrightness() {
  double old_percent = CalculateUndimmedBrightnessPercent();
  double new_percent =
      (old_percent < kMinVisiblePercent - kEpsilon) ? kMinVisiblePercent :
      ClampPercentToVisibleRange(old_percent + step_percent_);
  return SetUserBrightnessPercent(new_percent, TRANSITION_FAST);
}

bool InternalBacklightController::DecreaseUserBrightness(bool allow_off) {
  // Lower the backlight to the next step, turning it off if it was already at
  // the minimum visible level.
  double old_percent = CalculateUndimmedBrightnessPercent();
  double new_percent = old_percent <= kMinVisiblePercent + kEpsilon ? 0.0 :
      ClampPercentToVisibleRange(old_percent - step_percent_);

  if (!allow_off && new_percent <= kEpsilon) {
    user_adjustment_count_++;
    return false;
  }

  return SetUserBrightnessPercent(new_percent, TRANSITION_FAST);
}

int InternalBacklightController::GetNumAmbientLightSensorAdjustments() const {
  return als_adjustment_count_;
}

int InternalBacklightController::GetNumUserAdjustments() const {
  return user_adjustment_count_;
}

void InternalBacklightController::OnAmbientLightChanged(
    system::AmbientLightSensorInterface* sensor) {
  DCHECK_EQ(sensor, light_sensor_);

  if (ignore_ambient_light_)
    return;

  double percent = light_sensor_->GetAmbientLightPercent();
  if (percent < 0.0) {
    LOG(WARNING) << "ALS doesn't have valid value after sending "
                 << "OnAmbientLightChanged";
    return;
  }

  bool is_first_als_event = !has_seen_als_event_;
  als_offset_percent_ = percent;
  has_seen_als_event_ = true;

  // Force a backlight refresh immediately after returning from dim or idle.
  if (als_temporal_state_ == ALS_HYST_IMMEDIATE) {
    als_temporal_state_ = ALS_HYST_IDLE;
    als_adjustment_count_++;
    VLOG(1) << "Immediate ALS-triggered brightness adjustment";
    TransitionStyle transition =
        is_first_als_event ? TRANSITION_SLOW : TRANSITION_FAST;
    SetUndimmedBrightnessPercent(CalculateUndimmedBrightnessPercent(),
                                 transition,
                                 BRIGHTNESS_CHANGE_AUTOMATED);
    return;
  }

  // Apply level and temporal hysteresis to light sensor readings to reduce
  // backlight changes caused by minor and transient ambient light changes.
  double diff = percent - als_hysteresis_percent_;
  AlsHysteresisState new_state;

  if (diff < -kAlsHystPercent) {
    new_state = ALS_HYST_DOWN;
  } else if (diff > kAlsHystPercent) {
    new_state = ALS_HYST_UP;
  } else {
    als_temporal_state_ = ALS_HYST_IDLE;
    return;
  }
  if (als_temporal_state_ == new_state) {
    als_temporal_count_++;
  } else {
    als_temporal_state_ = new_state;
    als_temporal_count_ = 1;
  }
  if (als_temporal_count_ >= kAlsHystResponse) {
    als_temporal_count_ = 0;
    als_adjustment_count_++;
    VLOG(1) << "ALS-triggered adjustment; history (most recent first): "
            << light_sensor_->DumpPercentHistory();
    SetUndimmedBrightnessPercent(CalculateUndimmedBrightnessPercent(),
                                 TRANSITION_SLOW, BRIGHTNESS_CHANGE_AUTOMATED);
  }
}

double InternalBacklightController::CalculateUndimmedBrightnessPercent() const {
  double percent = power_source_ == POWER_AC ?
      plugged_offset_percent_ : unplugged_offset_percent_;
  percent += als_offset_percent_;
  return user_requested_zero_ ? 0.0 : ClampPercentToVisibleRange(percent);
}

void InternalBacklightController::EnsureUserBrightnessIsNonzero() {
  // Avoid turning the backlight back on if an external display is
  // connected since doing so may result in the desktop being resized.
  if (display_mode_ == DISPLAY_NORMAL &&
      CalculateUndimmedBrightnessPercent() < kMinVisiblePercent)
    IncreaseUserBrightness();
}

void InternalBacklightController::UpdateState() {
  // Hold off on changing the brightness at startup until all the required
  // state has been received.
  if (!has_seen_power_source_change_ || (light_sensor_ && !has_seen_als_event_))
    return;

  double brightness_percent = 100.0;
  TransitionStyle brightness_transition = TRANSITION_INSTANT;
  double resume_percent = -1.0;

  chromeos::DisplayPowerState display_power = chromeos::DISPLAY_POWER_ALL_ON;
  TransitionStyle display_transition = TRANSITION_INSTANT;
  bool set_display_power = true;

  if (shutting_down_) {
    brightness_percent = 0.0;
    display_power = chromeos::DISPLAY_POWER_ALL_OFF;
  } else if (suspended_) {
    brightness_percent = 0.0;
    resume_percent = CalculateUndimmedBrightnessPercent();
    // Chrome puts displays into the correct power state before suspend.
    set_display_power = false;
  } else if (off_for_inactivity_) {
    brightness_percent = 0.0;
    brightness_transition = TRANSITION_FAST;
    display_power = chromeos::DISPLAY_POWER_ALL_OFF;
    display_transition = TRANSITION_FAST;
  } else if (dimmed_for_inactivity_) {
    brightness_percent = std::min(CalculateUndimmedBrightnessPercent(),
                                  dimmed_brightness_percent_);
    brightness_transition = TRANSITION_FAST;
    display_power = chromeos::DISPLAY_POWER_ALL_ON;
    display_transition = TRANSITION_INSTANT;
  } else {
    brightness_percent = CalculateUndimmedBrightnessPercent();
    brightness_transition =
        (display_power_state_ != chromeos::DISPLAY_POWER_ALL_ON) ?
        TRANSITION_INSTANT : TRANSITION_FAST;
    // Turn the internal display off but leave external displays on if the
    // brightness has been reduced to 0.
    if (brightness_percent <= kEpsilon) {
      display_power = chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON;
      display_transition = TRANSITION_FAST;
    } else {
      display_power = chromeos::DISPLAY_POWER_ALL_ON;
      display_transition = TRANSITION_INSTANT;
    }
  }

  ApplyBrightnessPercent(brightness_percent, brightness_transition,
                         BRIGHTNESS_CHANGE_AUTOMATED);

  if (resume_percent >= 0.0)
    ApplyResumeBrightnessPercent(resume_percent);

  if (set_display_power) {
    SetDisplayPower(display_power,
                    TransitionStyleToTimeDelta(display_transition));
  }
}

bool InternalBacklightController::SetUndimmedBrightnessPercent(
    double percent,
    TransitionStyle style,
    BrightnessChangeCause cause) {
  percent = percent <= kEpsilon ? 0.0 : ClampPercentToVisibleRange(percent);

  if (cause == BRIGHTNESS_CHANGE_USER_INITIATED) {
    // Update the (possibly negative) user-contributed portion of the new
    // brightness by subtracting the ambient-light-sensor-contributed
    // portion.
    double* user_percent = (power_source_ == POWER_AC) ?
        &plugged_offset_percent_ : &unplugged_offset_percent_;
    *user_percent = percent - als_offset_percent_;
    // TODO(derat): This can be called many times in quick succession when
    // the user drags the brightness slider.  WritePrefs() should be called
    // asynchronously with a short timeout instead: http://crbug.com/196308
    WritePrefs();
  }

  // Use the current ambient light level as the benchmark for later readings.
  als_hysteresis_percent_ = als_offset_percent_;

  if (suspended_)
    ApplyResumeBrightnessPercent(percent);

  // Don't apply the change if we're in a state that overrides the new level.
  if (shutting_down_ || suspended_ || off_for_inactivity_ ||
      dimmed_for_inactivity_)
    return false;

  if (!ApplyBrightnessPercent(percent, style, cause))
    return false;

  // Turn the internal display off but leave external displays on if the
  // brightness has been reduced to 0.
  if (percent <= kEpsilon) {
    SetDisplayPower(
        chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
        TransitionStyleToTimeDelta(style) + turn_off_screen_timeout_);
  } else {
    SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON, base::TimeDelta());
  }
  return true;
}

bool InternalBacklightController::ApplyBrightnessPercent(
    double percent,
    TransitionStyle transition,
    BrightnessChangeCause cause) {
  int64 level = PercentToLevel(percent);
  if (level == current_level_)
    return false;

  // Force an instant transition if needed while moving within the
  // not-visible range.
  bool starting_below_min_visible_level = current_level_ < min_visible_level_;
  bool ending_below_min_visible_level = level < min_visible_level_;
  if (instant_transitions_below_min_level_ &&
      starting_below_min_visible_level != ending_below_min_visible_level)
    transition = TRANSITION_INSTANT;

  base::TimeDelta interval = TransitionStyleToTimeDelta(transition);
  VLOG(1) << "Setting brightness to " << level << " (" << percent
          << "%) over " << interval.InMilliseconds() << " ms";
  if (!backlight_->SetBrightnessLevel(level, interval)) {
    LOG(WARNING) << "Could not set brightness";
    return false;
  }

  current_level_ = level;
  FOR_EACH_OBSERVER(BacklightControllerObserver, observers_,
                    OnBrightnessChanged(percent, cause, this));
  return true;
}

bool InternalBacklightController::ApplyResumeBrightnessPercent(
    double resume_percent) {
  int64 level = PercentToLevel(resume_percent);
  VLOG(1) << "Setting resume brightness to " << level << " ("
          << resume_percent << "%)";
  return backlight_->SetResumeBrightnessLevel(level);
}

void InternalBacklightController::ReadPrefs() {
  if (!prefs_->GetInt64(kMinVisibleBacklightLevelPref, &min_visible_level_))
    min_visible_level_ = 1;
  min_visible_level_ = std::max(
      static_cast<int64>(
          lround(kDefaultMinVisibleBrightnessFraction * max_level_)),
      min_visible_level_);
  CHECK_GT(min_visible_level_, 0);
  min_visible_level_ = std::min(min_visible_level_, max_level_);

  CHECK(prefs_->GetDouble(kPluggedBrightnessOffsetPref,
                          &plugged_offset_percent_));
  CHECK(prefs_->GetDouble(kUnpluggedBrightnessOffsetPref,
                          &unplugged_offset_percent_));
  CHECK_GE(plugged_offset_percent_, -kMaxPercent);
  CHECK_LE(plugged_offset_percent_, kMaxPercent);
  CHECK_GE(unplugged_offset_percent_, -kMaxPercent);
  CHECK_LE(unplugged_offset_percent_, kMaxPercent);

  plugged_offset_percent_ =
      std::max(kMinVisiblePercent, plugged_offset_percent_);
  unplugged_offset_percent_ =
      std::max(kMinVisiblePercent, unplugged_offset_percent_);

  prefs_->GetBool(kInstantTransitionsBelowMinLevelPref,
                  &instant_transitions_below_min_level_);
  prefs_->GetBool(kDisableALSPref, &ignore_ambient_light_);

  int64 turn_off_screen_timeout_ms = 0;
  prefs_->GetInt64(kTurnOffScreenTimeoutMsPref, &turn_off_screen_timeout_ms);
  turn_off_screen_timeout_ =
      base::TimeDelta::FromMilliseconds(turn_off_screen_timeout_ms);
}

void InternalBacklightController::WritePrefs() {
  switch (power_source_) {
    case POWER_AC:
      prefs_->SetDouble(kPluggedBrightnessOffsetPref, plugged_offset_percent_);
      break;
    case POWER_BATTERY:
      prefs_->SetDouble(kUnpluggedBrightnessOffsetPref,
                        unplugged_offset_percent_);
      break;
  }
}

void InternalBacklightController::SetDisplayPower(
    chromeos::DisplayPowerState state,
    base::TimeDelta delay) {
  if (state == display_power_state_)
    return;

  display_power_setter_->SetDisplayPower(state, delay);
  display_power_state_ = state;
}

}  // namespace power_manager
