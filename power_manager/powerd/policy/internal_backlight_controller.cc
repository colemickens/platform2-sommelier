// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/internal_backlight_controller.h"

#include <sys/time.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "power_manager/common/clock.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/powerd/policy/backlight_controller_observer.h"
#include "power_manager/powerd/system/display_power_setter.h"

namespace power_manager {
namespace policy {

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
const int InternalBacklightController::kAmbientLightSensorTimeoutSec = 10;

InternalBacklightController::InternalBacklightController(
    system::BacklightInterface* backlight,
    PrefsInterface* prefs,
    system::AmbientLightSensorInterface* sensor,
    system::DisplayPowerSetterInterface* display_power_setter)
    : backlight_(backlight),
      prefs_(prefs),
      display_power_setter_(display_power_setter),
      ambient_light_handler_(
          sensor ? new AmbientLightHandler(sensor, this) : NULL),
      clock_(new Clock),
      power_source_(POWER_BATTERY),
      display_mode_(DISPLAY_NORMAL),
      dimmed_for_inactivity_(false),
      off_for_inactivity_(false),
      suspended_(false),
      shutting_down_(false),
      docked_(false),
      got_ambient_light_brightness_percent_(false),
      got_power_source_(false),
      als_adjustment_count_(0),
      user_adjustment_count_(0),
      ambient_light_brightness_percent_(kMaxPercent),
      plugged_user_brightness_percent_(kMaxPercent),
      unplugged_user_brightness_percent_(kMaxPercent),
      user_requested_zero_(false),
      max_level_(0),
      min_visible_level_(0),
      instant_transitions_below_min_level_(false),
      use_ambient_light_(true),
      step_percent_(1.0),
      dimmed_brightness_percent_(kDimmedBrightnessFraction * 100.0),
      level_to_percent_exponent_(kDefaultLevelToPercentExponent),
      current_level_(0),
      // Use all-off here to ensure that Init()'s all-on request is sent.
      display_power_state_(chromeos::DISPLAY_POWER_ALL_OFF) {
  DCHECK(backlight_);
  DCHECK(prefs_);
  DCHECK(display_power_setter_);
  if (ambient_light_handler_)
    ambient_light_handler_->set_name("panel");
}

InternalBacklightController::~InternalBacklightController() {}

bool InternalBacklightController::Init() {
  if (!backlight_->GetMaxBrightnessLevel(&max_level_) ||
      !backlight_->GetCurrentBrightnessLevel(&current_level_)) {
    LOG(ERROR) << "Querying backlight during initialization failed";
    return false;
  }

  if (!prefs_->GetInt64(kMinVisibleBacklightLevelPref, &min_visible_level_))
    min_visible_level_ = 1;
  min_visible_level_ = std::max(
      static_cast<int64>(
          lround(kDefaultMinVisibleBrightnessFraction * max_level_)),
      min_visible_level_);
  CHECK_GT(min_visible_level_, 0);
  min_visible_level_ = std::min(min_visible_level_, max_level_);

  const double initial_percent = LevelToPercent(current_level_);
  plugged_user_brightness_percent_ = initial_percent;
  unplugged_user_brightness_percent_ = initial_percent;
  prefs_->GetDouble(kInternalBacklightNoAlsAcBrightnessPref,
                    &plugged_user_brightness_percent_);
  prefs_->GetDouble(kInternalBacklightNoAlsBatteryBrightnessPref,
                    &unplugged_user_brightness_percent_);

  prefs_->GetBool(kInstantTransitionsBelowMinLevelPref,
                  &instant_transitions_below_min_level_);

  bool disable_als = false;
  prefs_->GetBool(kDisableALSPref, &disable_als);
  if (ambient_light_handler_ && !disable_als) {
    ambient_light_handler_->Init(prefs_, kInternalBacklightAlsLimitsPref,
        kInternalBacklightAlsStepsPref, initial_percent);
  } else {
    use_ambient_light_ = false;
  }

  int64 turn_off_screen_timeout_ms = 0;
  prefs_->GetInt64(kTurnOffScreenTimeoutMsPref, &turn_off_screen_timeout_ms);
  turn_off_screen_timeout_ =
      base::TimeDelta::FromMilliseconds(turn_off_screen_timeout_ms);

  if (max_level_ == min_visible_level_ || kMaxBrightnessSteps == 1) {
    step_percent_ = kMaxPercent;
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

  init_time_ = clock_->GetCurrentTime();
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
  if (got_power_source_ && power_source_ == source)
    return;

  VLOG(2) << "Power source changed to " << PowerSourceToString(source);

  // Ensure that the screen isn't dimmed in response to a transition to AC
  // or brightened in response to a transition to battery.
  if (got_power_source_) {
    bool plugged = source == POWER_AC;
    bool unplugged_exceeds_plugged =
        unplugged_user_brightness_percent_ > plugged_user_brightness_percent_;
    if (plugged && unplugged_exceeds_plugged)
      plugged_user_brightness_percent_ = unplugged_user_brightness_percent_;
    else if (!plugged && unplugged_exceeds_plugged)
      unplugged_user_brightness_percent_ = plugged_user_brightness_percent_;
  }

  power_source_ = source;
  got_power_source_ = true;
  UpdateState();
  if (ambient_light_handler_)
    ambient_light_handler_->HandlePowerSourceChange(source);
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

void InternalBacklightController::HandleUserActivity(UserActivityType type) {
  // Don't increase the brightness automatically when the user hits a
  // brightness key: if they hit brightness-up, IncreaseUserBrightness()
  // will be called soon anyway; if they hit brightness-down, the screen
  // shouldn't get turned back on.
  if (type != USER_ACTIVITY_BRIGHTNESS_UP_KEY_PRESS &&
      type != USER_ACTIVITY_BRIGHTNESS_DOWN_KEY_PRESS)
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

void InternalBacklightController::SetDocked(bool docked) {
  if (docked_ == docked)
    return;

  VLOG(2) << (docked ? "Entering" : "Leaving") << " docked mode";
  docked_ = docked;
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
  use_ambient_light_ = false;

  percent = percent <= kEpsilon ? 0.0 : ClampPercentToVisibleRange(percent);
  double* user_percent = (power_source_ == POWER_AC) ?
      &plugged_user_brightness_percent_ : &unplugged_user_brightness_percent_;
  *user_percent = percent;

  return UpdateUndimmedBrightness(style, BRIGHTNESS_CHANGE_USER_INITIATED);
}

bool InternalBacklightController::IncreaseUserBrightness() {
  double old_percent = GetUndimmedBrightnessPercent();
  double new_percent =
      (old_percent < kMinVisiblePercent - kEpsilon) ? kMinVisiblePercent :
      ClampPercentToVisibleRange(
          SnapBrightnessPercentToNearestStep(old_percent + step_percent_));
  return SetUserBrightnessPercent(new_percent, TRANSITION_FAST);
}

bool InternalBacklightController::DecreaseUserBrightness(bool allow_off) {
  // Lower the backlight to the next step, turning it off if it was already at
  // the minimum visible level.
  double old_percent = GetUndimmedBrightnessPercent();
  double new_percent = old_percent <= kMinVisiblePercent + kEpsilon ? 0.0 :
      ClampPercentToVisibleRange(
          SnapBrightnessPercentToNearestStep(old_percent - step_percent_));

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

void InternalBacklightController::SetBrightnessPercentForAmbientLight(
    double brightness_percent,
    AmbientLightHandler::BrightnessChangeCause cause) {
  ambient_light_brightness_percent_ = brightness_percent;
  got_ambient_light_brightness_percent_ = true;
  if (use_ambient_light_) {
    TransitionStyle transition =
        cause == AmbientLightHandler::CAUSED_BY_AMBIENT_LIGHT ?
        TRANSITION_SLOW : TRANSITION_FAST;
    UpdateUndimmedBrightness(transition, BRIGHTNESS_CHANGE_AUTOMATED);
  }
}

double InternalBacklightController::SnapBrightnessPercentToNearestStep(
    double percent) const {
  return round(percent / step_percent_) * step_percent_;
}

double InternalBacklightController::GetUndimmedBrightnessPercent() const {
  if (use_ambient_light_)
    return ClampPercentToVisibleRange(ambient_light_brightness_percent_);
  if (user_requested_zero_)
    return 0.0;
  return ClampPercentToVisibleRange(power_source_ == POWER_AC ?
      plugged_user_brightness_percent_ : unplugged_user_brightness_percent_);
}

void InternalBacklightController::EnsureUserBrightnessIsNonzero() {
  // Avoid turning the backlight back on if an external display is
  // connected since doing so may result in the desktop being resized.
  if (display_mode_ == DISPLAY_NORMAL &&
      GetUndimmedBrightnessPercent() < kMinVisiblePercent)
    IncreaseUserBrightness();
}

void InternalBacklightController::UpdateState() {
  // Give up on the ambient light sensor if it's not supplying readings.
  if (use_ambient_light_ && !got_ambient_light_brightness_percent_ &&
      clock_->GetCurrentTime() - init_time_ >=
      base::TimeDelta::FromSeconds(kAmbientLightSensorTimeoutSec)) {
    LOG(ERROR) << "Giving up on ambient light sensor after getting no reading "
               << "within " << kAmbientLightSensorTimeoutSec << " seconds";
    use_ambient_light_ = false;
  }

  // Hold off on changing the brightness at startup until all the required
  // state has been received.
  if (!got_power_source_ ||
      (use_ambient_light_ && !got_ambient_light_brightness_percent_))
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
    resume_percent = GetUndimmedBrightnessPercent();
    // Chrome puts displays into the correct power state before suspend.
    set_display_power = false;
  } else if (docked_) {
    brightness_percent = 0.0;
    display_power = chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON;
  } else if (off_for_inactivity_) {
    brightness_percent = 0.0;
    brightness_transition = TRANSITION_FAST;
    display_power = chromeos::DISPLAY_POWER_ALL_OFF;
    display_transition = TRANSITION_FAST;
  } else {
    brightness_percent = std::min(GetUndimmedBrightnessPercent(),
        dimmed_for_inactivity_ ? dimmed_brightness_percent_ : 100.0);
    const bool turning_on =
        display_power_state_ != chromeos::DISPLAY_POWER_ALL_ON ||
        current_level_ == 0;
    brightness_transition = turning_on ? TRANSITION_INSTANT : TRANSITION_FAST;

    // Keep the external display(s) on if the brightness was manually set to 0.
    display_power = (brightness_percent <= kEpsilon) ?
        chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON :
        chromeos::DISPLAY_POWER_ALL_ON;
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

bool InternalBacklightController::UpdateUndimmedBrightness(
    TransitionStyle style,
    BrightnessChangeCause cause) {
  const double percent = GetUndimmedBrightnessPercent();
  if (suspended_)
    ApplyResumeBrightnessPercent(percent);

  // Don't apply the change if we're in a state that overrides the new level.
  if (shutting_down_ || suspended_ || docked_ || off_for_inactivity_ ||
      dimmed_for_inactivity_)
    return false;

  if (!ApplyBrightnessPercent(percent, style, cause))
    return false;

  if (percent <= kEpsilon) {
    // Keep the external display(s) on if the brightness was manually set to 0.
    SetDisplayPower(
        chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
        docked_ ? base::TimeDelta() :
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

void InternalBacklightController::SetDisplayPower(
    chromeos::DisplayPowerState state,
    base::TimeDelta delay) {
  if (state == display_power_state_)
    return;

  display_power_setter_->SetDisplayPower(state, delay);
  display_power_state_ = state;
}

}  // namespace policy
}  // namespace power_manager
