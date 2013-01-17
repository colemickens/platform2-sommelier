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
#include "power_manager/common/util.h"
#include "power_manager/powerd/ambient_light_sensor.h"
#include "power_manager/powerd/monitor_reconfigure.h"

namespace {

// Minimum and maximum valid values for percentages.
const double kMinPercent = 0.0;
const double kMaxPercent = 100.0;

// When going into the idle-induced dim state, the backlight dims to this
// fraction (in the range [0.0, 1.0]) of its maximum brightness level.  This is
// a fraction rather than a percent so it won't change if
// kDefaultLevelToPercentExponent is modified.
const double kIdleBrightnessFraction = 0.1;

// Minimum brightness, as a fraction of the maximum level in the range [0.0,
// 1.0], that we'll remain at before turning the backlight off entirely.  This
// is arbitrarily chosen but seems to be a reasonable marginally-visible
// brightness for a darkened room on current devices: http://crosbug.com/24569.
// A higher level can be set via the kMinVisibleBacklightLevelPref setting.
// This is a fraction rather than a percent so it won't change if
// kDefaultLevelToPercentExponent is modified.
const double kDefaultMinVisibleBrightnessFraction = 0.0065;

// Total time that should be used to gradually animate the backlight level to a
// new brightness, in milliseconds.
const int64 kFastIntervalMs = 240;
const int64 kSlowIntervalMs = 2000;

// Maximum number of brightness adjustment steps.
const int64 kMaxBrightnessSteps = 16;

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

}  // namespace

namespace power_manager {

// static
const double InternalBacklightController::kMinVisiblePercent =
    kMaxPercent / kMaxBrightnessSteps;

InternalBacklightController::InternalBacklightController(
    system::BacklightInterface* backlight,
    PrefsInterface* prefs,
    AmbientLightSensor* sensor)
    : backlight_(backlight),
      prefs_(prefs),
      light_sensor_(sensor),
      monitor_reconfigure_(NULL),
      observer_(NULL),
      has_seen_als_event_(false),
      als_offset_percent_(0.0),
      als_hysteresis_percent_(0.0),
      als_temporal_state_(ALS_HYST_IMMEDIATE),
      als_adjustment_count_(0),
      user_adjustment_count_(0),
      plugged_offset_percent_(0.0),
      unplugged_offset_percent_(0.0),
      current_offset_percent_(&plugged_offset_percent_),
      state_(BACKLIGHT_UNINITIALIZED),
      plugged_state_(PLUGGED_STATE_UNKNOWN),
      target_percent_(0.0),
      max_level_(0),
      min_visible_level_(0),
      instant_transitions_below_min_level_(false),
      ignore_ambient_light_(false),
      step_percent_(1.0),
      idle_brightness_percent_(kIdleBrightnessFraction * 100.0),
      level_to_percent_exponent_(kDefaultLevelToPercentExponent),
      is_initialized_(false),
      target_level_(0),
      suspended_through_idle_off_(false),
      set_screen_power_state_timeout_id_(0),
      last_transition_style_(TRANSITION_INSTANT) {
  backlight_->set_observer(this);
}

InternalBacklightController::~InternalBacklightController() {
  util::RemoveTimeout(&set_screen_power_state_timeout_id_);
  backlight_->set_observer(NULL);
  if (light_sensor_) {
    light_sensor_->RemoveObserver(this);
    light_sensor_ = NULL;
  }
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

bool InternalBacklightController::Init() {
  if (light_sensor_)
    light_sensor_->AddObserver(this);
  if (!backlight_->GetMaxBrightnessLevel(&max_level_) ||
      !backlight_->GetCurrentBrightnessLevel(&target_level_)) {
    LOG(ERROR) << "Querying backlight during initialization failed";
    return false;
  }

  ReadPrefs();
  target_percent_ = LevelToPercent(target_level_);

  if (max_level_ == min_visible_level_ || kMaxBrightnessSteps == 1) {
    step_percent_ = 100.0;
  } else {
    // 1 is subtracted from kMaxBrightnessSteps to account for the step between
    // |min_brightness_level_| and 0.
    step_percent_ =
        (kMaxPercent - LevelToPercent(min_visible_level_)) /
        std::min(kMaxBrightnessSteps - 1, max_level_ - min_visible_level_);
  }
  CHECK_GT(step_percent_, 0.0);

  level_to_percent_exponent_ =
      max_level_ >= kMinLevelsForNonLinearMapping ?
          kDefaultLevelToPercentExponent :
          1.0;

  idle_brightness_percent_ = ClampPercentToVisibleRange(
      LevelToPercent(lround(kIdleBrightnessFraction * max_level_)));

  // If the current brightness is 0, the internal panel must be off.  Update
  // |monitor_reconfigure_| of this state so that it will properly turn on
  // the panel later.
  if (target_level_ == 0 && monitor_reconfigure_)
    monitor_reconfigure_->set_is_internal_panel_enabled(false);

  LOG(INFO) << "Backlight has range [0, " << max_level_ << "] with "
            << step_percent_ << "% step and minimum-visible level of "
            << min_visible_level_ << "; current level is " << target_level_
            << " (" << target_percent_ << "%)";
  is_initialized_ = true;
  return true;
}

void InternalBacklightController::SetMonitorReconfigure(
    MonitorReconfigureInterface* monitor_reconfigure) {
  monitor_reconfigure_ = monitor_reconfigure;
}

void InternalBacklightController::SetObserver(
    BacklightControllerObserver* observer) {
  observer_ = observer;
}

double InternalBacklightController::GetTargetBrightnessPercent() {
  return target_percent_;
}

bool InternalBacklightController::GetCurrentBrightnessPercent(double* percent) {
  DCHECK(percent);
  int64 level = 0;
  if (!backlight_->GetCurrentBrightnessLevel(&level))
    return false;

  *percent = LevelToPercent(level);
  return true;
}

bool InternalBacklightController::SetCurrentBrightnessPercent(
    double percent,
    BrightnessChangeCause cause,
    TransitionStyle style) {
  if (!is_initialized_)
    return false;

  percent = percent < 0.001 ? 0.0 : ClampPercentToVisibleRange(percent);
  if (percent == target_percent_)
    return false;
  *current_offset_percent_ = percent - als_offset_percent_;
  return WriteBrightness(true, cause, style);
}

bool InternalBacklightController::IncreaseBrightness(
    BrightnessChangeCause cause) {
  if (!is_initialized_)
    return false;

  double new_percent =
      target_percent_ < LevelToPercent(min_visible_level_) - 0.001 ?
      LevelToPercent(min_visible_level_) :
      ClampPercentToVisibleRange(target_percent_ + step_percent_);

  if (new_percent == target_percent_)
    return false;
  *current_offset_percent_ = new_percent - als_offset_percent_;
  return WriteBrightness(true, cause, TRANSITION_FAST);
}

bool InternalBacklightController::DecreaseBrightness(
    bool allow_off,
    BrightnessChangeCause cause) {
  if (!is_initialized_)
    return false;

  // Lower the backlight to the next step, turning it off if it was already at
  // the minimum visible level.
  double new_percent =
      target_percent_ <= LevelToPercent(min_visible_level_) + 0.001 ?
      0.0 :
      ClampPercentToVisibleRange(target_percent_ - step_percent_);

  if (new_percent == target_percent_ || (!allow_off && new_percent == 0))
    return false;
  *current_offset_percent_ = new_percent - als_offset_percent_;
  return WriteBrightness(true, cause, TRANSITION_FAST);
}

bool InternalBacklightController::SetPowerState(PowerState new_state) {
  if (new_state == state_ || !is_initialized_)
    return false;

  PowerState old_state = state_;

  CHECK(new_state != BACKLIGHT_UNINITIALIZED);

  // If backlight is turned off, do not transition to dim or off states.
  // From ACTIVE_OFF state only transition to ACTIVE and SUSPEND states.
  if (IsBacklightActiveOff() && (new_state == BACKLIGHT_IDLE_OFF ||
                                 new_state == BACKLIGHT_DIM ||
                                 new_state == BACKLIGHT_ALREADY_DIMMED))
    return false;

  LOG(INFO) << "Changing state: " << PowerStateToString(old_state) << " -> "
            << PowerStateToString(new_state);
  state_ = new_state;

  if (new_state == BACKLIGHT_SHUTTING_DOWN) {
    if (monitor_reconfigure_) {
      monitor_reconfigure_->SetScreenPowerState(
          OUTPUT_SELECTION_ALL_DISPLAYS, POWER_STATE_OFF);
    }
    return true;
  }

  if (old_state == BACKLIGHT_SHUTTING_DOWN) {
    LOG(WARNING) << "Unexpectedly transitioning out of shutting-down state";
    monitor_reconfigure_->SetScreenPowerState(
        OUTPUT_SELECTION_ALL_DISPLAYS, POWER_STATE_ON);
  }

  TransitionStyle style = TRANSITION_FAST;
  // Save the active backlight level if transitioning away from it.
  // Restore the saved value if returning to active state.
  if (old_state == BACKLIGHT_ACTIVE) {
    last_active_offset_percent_ = *current_offset_percent_;
  } else if (old_state != BACKLIGHT_UNINITIALIZED &&
             new_state == BACKLIGHT_ACTIVE) {
    double new_percent = ClampPercentToVisibleRange(
        last_active_offset_percent_ + als_offset_percent_);
    *current_offset_percent_ = new_percent - als_offset_percent_;

    // When waking up from IDLE_OFF, turn back on the screen.
    // When waking up from SUSPENDED:
    // 1. System was in IDLE_OFF before going to SUSPENDED. Kernel driver
    // will bring the system back to the IDLE_OFF state, and we need to
    // explicitly call SetScreenOn() to turn on the screen since user does
    // not expect to see blank screen.
    // 2. System went to SUSPENDED state directly. Kernel driver will bring
    // the system back to a non IDLE_OFF state, and we do nothing in this case.
    if (old_state == BACKLIGHT_IDLE_OFF ||
        (old_state == BACKLIGHT_SUSPENDED && suspended_through_idle_off_)) {
      SetScreenPowerState(OUTPUT_SELECTION_ALL_DISPLAYS, POWER_STATE_ON,
                          base::TimeDelta());
    }

    // If returning from suspend, force the backlight to zero to cancel out any
    // kernel driver behavior that sets it to some other value.  This allows
    // backlight controller to restore the backlight value from 0 to the saved
    // active level.
    if (old_state == BACKLIGHT_SUSPENDED)
      style = TRANSITION_INSTANT;
  } else if (old_state == BACKLIGHT_UNINITIALIZED &&
             new_state == BACKLIGHT_ACTIVE &&
             target_percent_ < LevelToPercent(min_visible_level_)) {
    // If this is the first-time initialization and the current brightness is
    // less than the minimum visible level, set brightness to the minimum level.
    // TODO(sque, crosbug.com/p/12243): Expand this to be called when session
    // is restarted.
    SetCurrentBrightnessPercent(LevelToPercent(min_visible_level_),
                                BRIGHTNESS_CHANGE_AUTOMATED,
                                TRANSITION_FAST);
  }

  if (new_state == BACKLIGHT_SUSPENDED) {
    style = TRANSITION_INSTANT;
    suspended_through_idle_off_ = (old_state == BACKLIGHT_IDLE_OFF);
  }

  bool write_brightness = true;
#ifdef HAS_ALS
  // For the first time SetPowerState() is called (when the system just
  // boots), if the ALS value has not been seen, skip the backlight adjustment.
  if (old_state == BACKLIGHT_UNINITIALIZED && !has_seen_als_event_) {
    LOG(INFO) << "First time SetPowerState() called, skip the backlight "
              << "brightness adjustment since no ALS value available yet.";
    write_brightness = false;
  }
#endif  // defined(HAS_ALS)
  if (write_brightness)
    WriteBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED, style);

  // Do not go to dim if backlight is already dimmed.
  if (new_state == BACKLIGHT_DIM && target_percent_ < idle_brightness_percent_)
    new_state = BACKLIGHT_ALREADY_DIMMED;

  als_temporal_state_ = ALS_HYST_IMMEDIATE;

  return true;
}

PowerState InternalBacklightController::GetPowerState() const {
  return state_;
}

bool InternalBacklightController::OnPlugEvent(bool is_plugged) {
  if (((is_plugged ? PLUGGED_STATE_CONNECTED : PLUGGED_STATE_DISCONNECTED) ==
       plugged_state_) ||
      !is_initialized_)
    return false;
  bool is_first_time = (plugged_state_ == PLUGGED_STATE_UNKNOWN);
  if (is_plugged) {
    current_offset_percent_ = &plugged_offset_percent_;
    plugged_state_ = PLUGGED_STATE_CONNECTED;
    // If unplugged brightness is set to greater than plugged brightness,
    // increase the plugged brightness so that it is not less than unplugged
    // brightness.  Otherwise there will be an unnatural decrease in brightness
    // when the user switches from battery to AC power.
    // If the backlight is in active-but-off state, plugging in AC power
    // shouldn't exit the state.  The plugged brightness should be set to off as
    // well.
    if (!is_first_time &&
        (IsBacklightActiveOff() ||
         unplugged_offset_percent_ > plugged_offset_percent_)) {
      plugged_offset_percent_ = unplugged_offset_percent_;
    }
  } else {
    current_offset_percent_ = &unplugged_offset_percent_;
    plugged_state_ = PLUGGED_STATE_DISCONNECTED;
    // If plugged brightness is set to less than unplugged brightness, reduce
    // the unplugged brightness so that it is not greater than plugged
    // brightness.  Otherwise there will be an unnatural increase in brightness
    // when the user switches from AC to battery power.
    if (!is_first_time && (plugged_offset_percent_ < unplugged_offset_percent_))
      unplugged_offset_percent_ = plugged_offset_percent_;
  }

  // Adjust new offset to make sure the plug/unplug transition doesn't turn off
  // the screen.  If the backlight is in active-but-off state, don't make this
  // adjustment.
  if (!IsBacklightActiveOff() &&
      *current_offset_percent_ + als_offset_percent_ < 1)
    *current_offset_percent_ = 1 - als_offset_percent_;

#ifdef HAS_ALS
  // For the first time OnPlugEvent() is called (when the system just boots),
  // if the ALS value has not been seen, skip the backlight adjustment.
  if (is_first_time) {
    if (has_seen_als_event_) {
      return WriteBrightness(
        true, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_FAST);
    }
    LOG(INFO) << "First time OnPlugEvent() called, skip the backlight "
              << "brightness adjustment since no ALS value available yet.";
    return true;
  }
#endif  // defined(HAS_ALS)
  return WriteBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_FAST);
}

bool InternalBacklightController::IsBacklightActiveOff() {
  return state_ == BACKLIGHT_ACTIVE && target_percent_ == 0;
}

int InternalBacklightController::GetNumAmbientLightSensorAdjustments() const {
  return als_adjustment_count_;
}

int InternalBacklightController::GetNumUserAdjustments() const {
  return user_adjustment_count_;
}

void InternalBacklightController::OnBacklightDeviceChanged() {
  LOG(INFO) << "Backlight device changed; reinitializing controller";
  if (Init())
    WriteBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_FAST);
}

void InternalBacklightController::OnAmbientLightChanged(
    AmbientLightSensor* sensor) {
#ifndef HAS_ALS
  LOG(WARNING) << "Got ALS reading from platform supposed to have no ALS. "
               << "Please check the platform ALS configuration.";
#endif
  DCHECK_EQ(sensor, light_sensor_);

  if (ignore_ambient_light_)
    return;

  double percent = light_sensor_->GetAmbientLightPercent();
  if (percent < 0.0) {
    LOG(WARNING) << "ALS doesn't have valid value after sending "
                 << "OnAmbientLightChanged";
    return;
  }

  if (!is_initialized_)
    return;

  // Do not use ALS to adjust the backlight brightness if the backlight is
  // turned off.
  if (state_ == BACKLIGHT_IDLE_OFF || IsBacklightActiveOff())
    return;

  bool is_first_als_event = !has_seen_als_event_;
  als_offset_percent_ = percent;
  has_seen_als_event_ = true;

  // Force a backlight refresh immediately after returning from dim or idle.
  if (als_temporal_state_ == ALS_HYST_IMMEDIATE) {
    als_temporal_state_ = ALS_HYST_IDLE;
    als_adjustment_count_++;
    LOG(INFO) << "Immediate ALS-triggered brightness adjustment.";
    WriteBrightness(false, BRIGHTNESS_CHANGE_AUTOMATED,
                    is_first_als_event ? TRANSITION_SLOW : TRANSITION_FAST);
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
    LOG(INFO) << "Ambient light sensor-triggered brightness adjustment.";
    LOG(INFO) << "ALS history (most recent first): "
              << light_sensor_->DumpPercentHistory();
    // ALS adjustment should not change brightness offset.
    WriteBrightness(false, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_SLOW);
  }
}

double InternalBacklightController::ClampPercentToVisibleRange(double percent) {
  return std::min(kMaxPercent, std::max(kMinVisiblePercent, percent));
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

  double min_percent = LevelToPercent(min_visible_level_);
  plugged_offset_percent_ = std::max(min_percent, plugged_offset_percent_);
  unplugged_offset_percent_ = std::max(min_percent, unplugged_offset_percent_);

  prefs_->GetBool(kInstantTransitionsBelowMinLevelPref,
                  &instant_transitions_below_min_level_);
  prefs_->GetBool(kDisableALSPref, &ignore_ambient_light_);

  int64 turn_off_screen_timeout_ms = 0;
  prefs_->GetInt64(kTurnOffScreenTimeoutMsPref, &turn_off_screen_timeout_ms);
  turn_off_screen_timeout_ =
      base::TimeDelta::FromMilliseconds(turn_off_screen_timeout_ms);
}

void InternalBacklightController::WritePrefs() {
  if (!is_initialized_)
    return;
  if (plugged_state_ == PLUGGED_STATE_CONNECTED) {
    prefs_->SetDouble(kPluggedBrightnessOffsetPref, plugged_offset_percent_);
  } else if (plugged_state_ == PLUGGED_STATE_DISCONNECTED) {
    prefs_->SetDouble(kUnpluggedBrightnessOffsetPref,
                      unplugged_offset_percent_);
  }
}

bool InternalBacklightController::WriteBrightness(bool adjust_brightness_offset,
                                                  BrightnessChangeCause cause,
                                                  TransitionStyle style) {
  if (!is_initialized_)
    return false;

  if (cause == BRIGHTNESS_CHANGE_USER_INITIATED)
    user_adjustment_count_++;

  double old_percent = target_percent_;
  if (state_ == BACKLIGHT_ACTIVE || state_ == BACKLIGHT_ALREADY_DIMMED) {
    double new_percent = als_offset_percent_ + *current_offset_percent_;
    target_percent_ =
        (new_percent <= 0.001) ? 0.0 : ClampPercentToVisibleRange(new_percent);

    // Do not turn off backlight if this is a "soft" adjustment -- e.g. due to
    // ALS change.
    // Also, do not turn off the backlight if it has been dimmed and idled.
    //
    // Note that when adjusting during idle events, such as from screen-off or
    // suspend to active, it's an automated change, but adjust_brightness_offset
    // is allowed.  Brightness needs to be set from zero to nonzero.
    if (state_ == BACKLIGHT_ALREADY_DIMMED ||
        (!adjust_brightness_offset && cause == BRIGHTNESS_CHANGE_AUTOMATED)) {
      if (target_percent_ == 0.0 && old_percent > 0.0)
        target_percent_ = std::max(LevelToPercent(min_visible_level_), 1.0);
      else if (target_percent_ > 0.0 && old_percent == 0.0)
        target_percent_ = 0.0;
    }
    // Adjust offset in case brightness was changed.
    if (adjust_brightness_offset)
      *current_offset_percent_ = target_percent_ - als_offset_percent_;
  } else if (state_ == BACKLIGHT_DIM) {
    // When in dimmed state, set to dim level only if it results in a reduction
    // of system brightness.  Also, make sure idle brightness is not lower than
    // the minimum brightness.
    if (old_percent > idle_brightness_percent_) {
      target_percent_ = idle_brightness_percent_;
    } else {
      LOG(INFO) << "Not dimming because backlight is already dim.";
      // Even if the brightness is below the dim level, make sure it is not
      // below the minimum brightness.
      target_percent_ = ClampPercentToVisibleRange(target_percent_);
    }
  } else if (state_ == BACKLIGHT_IDLE_OFF) {
    target_percent_ = 0;
  } else if (state_ == BACKLIGHT_SUSPENDED) {
    // If we're about to suspend, set the resume backlight level so that the
    // backlight driver will use it when it turns the display back on after
    // resume, then turn the backlight off.
    als_hysteresis_percent_ = als_offset_percent_;
    target_percent_ = last_active_offset_percent_;
    int64 level = PercentToLevel(target_percent_);
    LOG(INFO) << "Set resume brightness: " << target_percent_ << "%";
    backlight_->SetResumeBrightnessLevel(level);
    SetBrightness(PercentToLevel(0), TRANSITION_INSTANT);
    return true;
  }

  als_hysteresis_percent_ = als_offset_percent_;
  int64 level = PercentToLevel(target_percent_);
  LOG(INFO) << "WriteBrightness: " << old_percent << "% -> "
            << target_percent_ << "%";
  if (SetBrightness(level, style)) {
    WritePrefs();
    if (observer_)
      observer_->OnBrightnessChanged(target_percent_, cause, this);
  }

  return true;
}

bool InternalBacklightController::SetBrightness(int64 target_level,
                                                TransitionStyle style) {
  LOG(INFO) << "Setting brightness level to " << target_level
            << " (previous target was " << target_level_ << ")";

  // If this is a redundant call (existing target level is the same as
  // new target level), ignore this call.
  if (target_level_ == target_level)
    return true;

  int64 old_target_level = target_level_;
  target_level_ = target_level;

  // If we don't want to spend any time in (0, min_visible_level_), then do an
  // instant transition.
  bool starting_below_min_visible_level = old_target_level < min_visible_level_;
  bool ending_below_min_visible_level = target_level < min_visible_level_;
  if (instant_transitions_below_min_level_ &&
      starting_below_min_visible_level != ending_below_min_visible_level)
    style = TRANSITION_INSTANT;

  last_transition_style_ = style;


  base::TimeDelta interval;
  switch (style) {
    case TRANSITION_INSTANT:
      break;
    case TRANSITION_FAST:
      interval = base::TimeDelta::FromMilliseconds(kFastIntervalMs);
      break;
    case TRANSITION_SLOW:
      interval = base::TimeDelta::FromMilliseconds(kSlowIntervalMs);
      break;
    default:
      NOTREACHED() << "Unhandled transition style " << style;
  }

  if (target_level == 0) {
    // If we're turning off the backlight, make sure that the screen gets
    // turned off when the backlight hits 0.  If we're in the "active" state
    // (i.e. we're turning the backlight off because the user hit the
    // brightness-down key instead of due to idleness), then we turn off the
    // internal panel but leave external displays on; perhaps the user is
    // using an external display and wants to turn off just the backlight to
    // save power.  We don't do anything in the SUSPENDED state since the
    // kernel driver should turn off the screen.
    if (state_ == BACKLIGHT_IDLE_OFF) {
      SetScreenPowerState(OUTPUT_SELECTION_ALL_DISPLAYS, POWER_STATE_OFF,
                          interval);
    } else if (state_ == BACKLIGHT_ACTIVE) {
      SetScreenPowerState(OUTPUT_SELECTION_INTERNAL_ONLY, POWER_STATE_OFF,
                          interval + turn_off_screen_timeout_);
    }
  } else if (old_target_level == 0 && state_ != BACKLIGHT_SUSPENDED) {
    // If we're about to suspend, don't turn the internal panel on -- we're just
    // setting the brightness so that it'll be at the right level immediately
    SetScreenPowerState(OUTPUT_SELECTION_INTERNAL_ONLY, POWER_STATE_ON,
                        base::TimeDelta());
  }

  if (!backlight_->SetBrightnessLevel(target_level, interval))
    DLOG(INFO) << "Could not set brightness to " << target_level;
  return true;
}

void InternalBacklightController::SetScreenPowerState(
    ScreenPowerOutputSelection selection,
    ScreenPowerState state,
    base::TimeDelta delay) {
  if (!monitor_reconfigure_)
    return;

  util::RemoveTimeout(&set_screen_power_state_timeout_id_);
  if (delay.InMilliseconds() == 0) {
    monitor_reconfigure_->SetScreenPowerState(selection, state);
  } else {
    set_screen_power_state_timeout_id_ =
        g_timeout_add(delay.InMilliseconds(),
                      HandleSetScreenPowerStateTimeoutThunk,
                      CreateHandleSetScreenPowerStateTimeoutArgs(
                          this, selection, state));
  }
}

gboolean InternalBacklightController::HandleSetScreenPowerStateTimeout(
    ScreenPowerOutputSelection selection,
    ScreenPowerState state) {
  monitor_reconfigure_->SetScreenPowerState(selection, state);
  set_screen_power_state_timeout_id_ = 0;
  return FALSE;
}

}  // namespace power_manager
