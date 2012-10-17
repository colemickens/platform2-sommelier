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
#include "power_manager/common/power_prefs_interface.h"
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

// Gradually animate backlight level to new brightness by breaking up the
// transition into multiple steps.
const int kFastBacklightAnimationFrames = 8;
const int kSlowBacklightAnimationFrames = 66;

// Time between backlight animation frames, in milliseconds.
const int kBacklightAnimationMs = 30;

// Amount of variation allowed in actual animation frame intervals.
const double kBacklightAnimationTolerance = 0.2;

// Minimum and maximum allowed animation frame interval, in milliseconds.
// Anything outside of it should result in a warning being logged.
const int kBacklightAnimationMinMs = static_cast<int>(
    (1.0 - kBacklightAnimationTolerance) * kBacklightAnimationMs);
const int kBacklightAnimationMaxMs = static_cast<int>(
    (1.0 + kBacklightAnimationTolerance) * kBacklightAnimationMs);

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
    BacklightInterface* backlight,
    PowerPrefsInterface* prefs,
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
      plugged_state_(kPowerUnknown),
      target_percent_(0.0),
      max_level_(0),
      min_visible_level_(0),
      instant_transitions_below_min_level_(false),
      step_percent_(1.0),
      idle_brightness_percent_(kIdleBrightnessFraction * 100.0),
      level_to_percent_exponent_(kDefaultLevelToPercentExponent),
      is_initialized_(false),
      target_level_(0),
      controller_factor_(0),
      suspended_through_idle_off_(false),
      gradual_transition_event_id_(0) {
  backlight_->set_observer(this);
}

InternalBacklightController::~InternalBacklightController() {
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
  if (!backlight_->GetMaxBrightnessLevel(&max_level_)) {
    LOG(ERROR) << "Querying max backlight during initialization failed";
    return false;
  }

  ReadPrefs();
  if (!GetCurrentControllerLevel(&target_level_)) {
    LOG(ERROR) << "Querying current backlight during initialization failed";
    return false;
  }
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
    MonitorReconfigure* monitor_reconfigure) {
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
  if (!GetCurrentControllerLevel(&level))
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

  state_ = new_state;

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
        (old_state == BACKLIGHT_SUSPENDED && suspended_through_idle_off_))
      if (monitor_reconfigure_) {
        monitor_reconfigure_->SetScreenPowerState(OUTPUT_SELECTION_ALL_DISPLAYS,
                                                  POWER_STATE_ON);
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

  LOG(INFO) << PowerStateToString(old_state) << " -> "
            << PowerStateToString(new_state);
  return true;
}

PowerState InternalBacklightController::GetPowerState() const {
  return state_;
}

bool InternalBacklightController::OnPlugEvent(bool is_plugged) {
  if ((is_plugged ? kPowerConnected : kPowerDisconnected) == plugged_state_ ||
      !is_initialized_)
    return false;
  bool is_first_time = (plugged_state_ == kPowerUnknown);
  if (is_plugged) {
    current_offset_percent_ = &plugged_offset_percent_;
    plugged_state_ = kPowerConnected;
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
    plugged_state_ = kPowerDisconnected;
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

  if (light_sensor_ != sensor) {
    LOG(WARNING) << "Received AmbientLightChange from unknown sensor";
    return;
  }

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

  als_offset_percent_ = percent;
  has_seen_als_event_ = true;

  // Force a backlight refresh immediately after returning from dim or idle.
  if (als_temporal_state_ == ALS_HYST_IMMEDIATE) {
    als_temporal_state_ = ALS_HYST_IDLE;
    als_adjustment_count_++;
    LOG(INFO) << "Immediate ALS-triggered brightness adjustment.";
    WriteBrightness(false, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_FAST);
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
  int64 controller_levels;
  if (prefs_->GetInt64(kInternalBacklightControllerLevelsPref,
                       &controller_levels) && controller_levels > 0) {
    if (controller_levels > max_level_ / 2) {
      LOG(WARNING) << "Unable to implement " << controller_levels <<
                   " controller backlight levels with max_brightness " <<
                   max_level_;
    } else {
      LOG(INFO) << "Using " << controller_levels <<
                   " controller backlight levels with max_brightness " <<
                   max_level_;
      // Change max_level_ from sysfs context (ie, directly from the sysfs
      // max_brightness file) to controller context (ie, from the prefs file).
      controller_factor_ = max_level_ / controller_levels;
      max_level_ = controller_levels - 1;
    }
  }

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

  int64 instant_transition_pref = false;
  prefs_->GetInt64(kInstantTransitionsBelowMinLevelPref,
                   &instant_transition_pref);
  instant_transitions_below_min_level_ = (instant_transition_pref != 0);
}

void InternalBacklightController::WritePrefs() {
  if (!is_initialized_)
    return;
  if (plugged_state_ == kPowerConnected) {
    prefs_->SetDouble(kPluggedBrightnessOffsetPref, plugged_offset_percent_);
  } else if (plugged_state_ == kPowerDisconnected) {
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
  } else if (state_ == BACKLIGHT_IDLE_OFF || state_ == BACKLIGHT_SUSPENDED) {
    target_percent_ = 0;
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
  int64 current_level = 0;
  GetCurrentControllerLevel(&current_level);
  LOG(INFO) << "Setting brightness level to " << target_level
            << " (currently " << current_level << ", previous target was "
            << target_level_ << ")";

  // If this is a redundant call (existing target level is the same as
  // new target level), ignore this call.
  if (target_level_ == target_level)
    return true;

  // Otherwise, set to the new target brightness. This will disable any
  // outstanding brightness transition to a different brightness.
  target_level_ = target_level;

  // Stop existing gradual brightness transition if there is one.
  if (gradual_transition_event_id_ > 0) {
    g_source_remove(gradual_transition_event_id_);
    gradual_transition_event_id_ = 0;
  }

  // If the current brightness happens to be at the new target brightness,
  // do not start a new transition.
  int64 diff = target_level - current_level;
  if (diff == 0)
    return true;

  // If the transition is from 0 to |min_visible_level_|, do it instantly to
  // avoid setting backlight to somewhere in the middle of that range.
  if (current_level < target_level && target_level == min_visible_level_ &&
      instant_transitions_below_min_level_) {
    style = TRANSITION_INSTANT;
  }

  if (style == TRANSITION_INSTANT) {
    SetBrightnessHard(target_level, target_level);
    return true;
  }

  // The following check will require this code to be updated should more styles
  // be added in the future.
  DCHECK(style == TRANSITION_FAST || style == TRANSITION_SLOW);

  // We don't want to take more steps than there are adjustment levels between
  // the start brightness and the end brightness.
  int num_frames = style == TRANSITION_FAST ? kFastBacklightAnimationFrames :
                                              kSlowBacklightAnimationFrames;
  int64 num_steps = std::min(abs(diff), num_frames);
  if (num_steps <= 1) {
    SetBrightnessHard(target_level, target_level);
    return true;
  }

  gradual_transition_total_time_ = base::TimeDelta::FromMilliseconds(
      (num_steps - 1) * kBacklightAnimationMs);
  gradual_transition_start_level_ = current_level +
    static_cast<int64>(round(static_cast<double>(diff) / num_steps));

  // The first adjustment step should happen immediately.  Don't use a timeout
  // for this.
  gradual_transition_start_time_ = base::TimeTicks::Now();
  SetBrightnessHard(gradual_transition_start_level_, target_level);
  gradual_transition_last_step_time_ = gradual_transition_start_time_;

  gradual_transition_event_id_ = g_timeout_add(kBacklightAnimationMs,
                                               SetBrightnessStepThunk,
                                               this);
  CHECK_GT(gradual_transition_event_id_, 0U);
  return true;
}

gboolean InternalBacklightController::SetBrightnessStep() {
  // Determine the current step brightness using linear interpolation based on
  // how much of the expected transition time has already elapsed.
  base::TimeTicks current_time = base::TimeTicks::Now();
  double elapsed_time_fraction =
      (current_time - gradual_transition_start_time_).InMillisecondsF() /
      gradual_transition_total_time_.InMillisecondsF();
  elapsed_time_fraction = std::min(1.0, elapsed_time_fraction);
  int64 current_brightness = static_cast<int64>(
      round(gradual_transition_start_level_ + elapsed_time_fraction *
            (target_level_ - gradual_transition_start_level_)));

  // If instant transitions are required below |min_visible_level_|, and this is
  // a transition from >= |min_visible_level_| to 0, skip all steps below the
  // min level.  This allows for a delay before turning off the screen, so the
  // UI can show the slider going to 0.
  bool skip_transition_step =
      current_brightness < min_visible_level_ &&
      current_brightness != 0 &&
      instant_transitions_below_min_level_;
  if (!skip_transition_step)
    SetBrightnessHard(current_brightness, target_level_);

  // Add a check for transition intervals that are too long or too short.
  // Too long means the transition events are blocked by other events.
  // Too short means either the event timing is broken or the last step time is
  // not being reset on a new transition.
  int64 diff_ms =
      (current_time - gradual_transition_last_step_time_).InMilliseconds();
  if (diff_ms > kBacklightAnimationMaxMs) {
    LOG(WARNING) << "Interval between adjustment steps was " << diff_ms
                 << " ms, expected no more than " << kBacklightAnimationMaxMs
                 << " ms";
  } else if (diff_ms < kBacklightAnimationMinMs) {
    LOG(WARNING) << "Interval between adjustment steps was " << diff_ms
                 << " ms, expected no less than " << kBacklightAnimationMinMs
                 << " ms";
  }
  gradual_transition_last_step_time_ = current_time;

  if (elapsed_time_fraction >= 1.0) {
    // When there are no more adjustment steps to take, the transition ends.
    gradual_transition_event_id_ = 0;
    return false;
  }
  return true;
}

void InternalBacklightController::SetBrightnessHard(int64 level,
                                                    int64 target_level) {
  if (level != 0 && target_level != 0 && monitor_reconfigure_)
    monitor_reconfigure_->SetScreenPowerState(OUTPUT_SELECTION_INTERNAL_ONLY,
                                              POWER_STATE_ON);

  if (!SetCurrentControllerLevel(level))
    DLOG(INFO) << "Could not set brightness to " << level;

  if (level == 0 && target_level == 0 && monitor_reconfigure_) {
    if (state_ == BACKLIGHT_IDLE_OFF) {
      // If it is in IDLE_OFF state, we call SetScreenOff() to turn off all the
      // display outputs. We don't do anything in the SUSPENDED state since
      // kernel driver will turn off the screen.
      monitor_reconfigure_->SetScreenPowerState(OUTPUT_SELECTION_ALL_DISPLAYS,
                                                POWER_STATE_OFF);
    } else if (state_ == BACKLIGHT_ACTIVE) {
      // If backlight is 0 but we are in ACTIVE state, we turn off the internal
      // panel only -- the user might be using an external monitor and
      // attempting to turn off just the internal backlight to conserve power.
      monitor_reconfigure_->SetScreenPowerState(OUTPUT_SELECTION_INTERNAL_ONLY,
                                                POWER_STATE_OFF);
    }
  }
}

bool InternalBacklightController::GetCurrentControllerLevel(int64* level) {
  if (!backlight_->GetCurrentBrightnessLevel(level))
    return false;
  if (controller_factor_ > 0 && *level != 0) {
    *level -= controller_factor_ / 2;
    *level /= controller_factor_;
  }
  return true;
}

bool InternalBacklightController::SetCurrentControllerLevel(int64 level) {
  if (controller_factor_ > 0 && level != 0) {
    level *= controller_factor_;
    level += controller_factor_ / 2;
  }
  return backlight_->SetBrightnessLevel(level);
}

}  // namespace power_manager
