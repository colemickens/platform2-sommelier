// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/backlight_controller.h"

#include <algorithm>
#include <cmath>
#include <gdk/gdkx.h>
#include <sys/time.h>
#include <X11/extensions/dpms.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

#include "power_manager/ambient_light_sensor.h"
#include "power_manager/monitor_reconfigure.h"
#include "power_manager/power_constants.h"
#include "power_manager/power_prefs_interface.h"
#include "power_manager/util.h"


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
// A higher level can be set via the kMinVisibleBacklightLevel setting.  This is
// a fraction rather than a percent so it won't change if
// kDefaultLevelToPercentExponent is modified.
const double kDefaultMinVisibleBrightnessFraction = 0.0065;

// Gradually animate backlight level to new brightness by breaking up the
// transition into this many steps.
const int kBacklightAnimationFrames = 8;

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
const int kAlsHystResponse = 4;

// Backlight change (in %) required to overcome light sensor level hysteresis.
const int kAlsHystPercent = 5;

// Value for |level_to_percent_exponent_|, assuming that at least
// |kMinLevelsForNonLinearScale| brightness levels are available -- if not, we
// just use 1.0 to give us a linear scale.
const double kDefaultLevelToPercentExponent = 0.5;

// Minimum number of brightness levels needed before we use a non-linear mapping
// between levels and percents.
const double kMinLevelsForNonLinearMapping = 100;

// String names for power states.
const char* PowerStateToString(power_manager::PowerState state) {
  switch (state) {
    case power_manager::BACKLIGHT_ACTIVE:
      return "state(ACTIVE)";
    case power_manager::BACKLIGHT_DIM:
      return "state(DIM)";
    case power_manager::BACKLIGHT_ALREADY_DIMMED:
      return "state(ALREADY_DIMMED)";
    case power_manager::BACKLIGHT_IDLE_OFF:
      return "state(IDLE_OFF)";
    case power_manager::BACKLIGHT_SUSPENDED:
      return "state(SUSPENDED)";
    case power_manager::BACKLIGHT_UNINITIALIZED:
      return "state(UNINITIALIZED)";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

namespace power_manager {

BacklightController::BacklightController(BacklightInterface* backlight,
                                         PowerPrefsInterface* prefs)
    : backlight_(backlight),
      prefs_(prefs),
      light_sensor_(NULL),
      monitor_reconfigure_(NULL),
      observer_(NULL),
      has_seen_als_event_(false),
      als_offset_percent_(0.0),
      als_hysteresis_percent_(0.0),
      als_temporal_state_(ALS_HYST_IMMEDIATE),
      als_adjustment_count_(0),
      user_adjustment_count_(0),
      als_response_index_(0),
      plugged_offset_percent_(0.0),
      unplugged_offset_percent_(0.0),
      current_offset_percent_(&plugged_offset_percent_),
      state_(BACKLIGHT_UNINITIALIZED),
      plugged_state_(kPowerUnknown),
      target_percent_(0.0),
      max_level_(0),
      min_visible_level_(0),
      step_percent_(1.0),
      idle_brightness_percent_(kIdleBrightnessFraction * 100.0),
      level_to_percent_exponent_(kDefaultLevelToPercentExponent),
      is_initialized_(false),
      target_level_(0),
      suspended_through_idle_off_(false),
      gradual_transition_event_id_(0) {
  for (size_t i = 0; i < arraysize(als_responses_); i++)
    als_responses_[i] = 0;
  backlight_->set_observer(this);
}

BacklightController::~BacklightController() {
  backlight_->set_observer(NULL);
}

bool BacklightController::Init() {
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

  LOG(INFO) << "Backlight has range [0, " << max_level_ << "] with "
            << step_percent_ << "% step and minimum-visible level of "
            << min_visible_level_ << "; current level is " << target_level_
            << " (" << target_percent_ << "%)";
  is_initialized_ = true;
  return true;
}

bool BacklightController::GetCurrentBrightnessPercent(double* percent) {
  DCHECK(percent);
  int64 level = 0;
  if (!backlight_->GetCurrentBrightnessLevel(&level))
    return false;

  *percent = LevelToPercent(level);
  return true;
}

bool BacklightController::SetCurrentBrightnessPercent(
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

bool BacklightController::IncreaseBrightness(BrightnessChangeCause cause) {
  if (!is_initialized_)
    return false;

  double new_percent =
      target_percent_ < LevelToPercent(min_visible_level_) - 0.001 ?
      LevelToPercent(min_visible_level_) :
      ClampPercentToVisibleRange(target_percent_ + step_percent_);

  if (new_percent == target_percent_)
    return false;
  *current_offset_percent_ = new_percent - als_offset_percent_;
  return WriteBrightness(true, cause, TRANSITION_GRADUAL);
}

bool BacklightController::DecreaseBrightness(bool allow_off,
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
  return WriteBrightness(true, cause, TRANSITION_GRADUAL);
}

bool BacklightController::SetPowerState(PowerState new_state) {
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

  TransitionStyle style = TRANSITION_GRADUAL;
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
      if (monitor_reconfigure_)
        monitor_reconfigure_->SetScreenOn();

    // If returning from suspend, force the backlight to zero to cancel out any
    // kernel driver behavior that sets it to some other value.  This allows
    // backlight controller to restore the backlight value from 0 to the saved
    // active level.
    if (old_state == BACKLIGHT_SUSPENDED)
      style = TRANSITION_INSTANT;
  }

  if (new_state == BACKLIGHT_SUSPENDED) {
    style = TRANSITION_INSTANT;
    suspended_through_idle_off_ = old_state == BACKLIGHT_IDLE_OFF;
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
#endif // defined(HAS_ALS)
  if (write_brightness)
    WriteBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED, style);

  // Do not go to dim if backlight is already dimmed.
  if (new_state == BACKLIGHT_DIM && target_percent_ < idle_brightness_percent_)
    new_state = BACKLIGHT_ALREADY_DIMMED;

  if (light_sensor_)
    light_sensor_->EnableOrDisableSensor(state_);
  als_temporal_state_ = ALS_HYST_IMMEDIATE;

  LOG(INFO) << PowerStateToString(old_state) << " -> "
            << PowerStateToString(new_state);
  return true;
}

bool BacklightController::OnPlugEvent(bool is_plugged) {
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
        true, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_GRADUAL);
    }
    LOG(INFO) << "First time OnPlugEvent() called, skip the backlight "
              << "brightness adjustment since no ALS value available yet.";
    return true;
  }
#endif // defined(HAS_ALS)
  return WriteBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_GRADUAL);
}

void BacklightController::SetAlsBrightnessOffsetPercent(double percent) {
#ifndef HAS_ALS
  LOG(WARNING) << "Got ALS reading from platform supposed to have no ALS. "
               << "Please check the platform ALS configuration.";
#endif

  if (!is_initialized_)
    return;

  // Do not use ALS to adjust the backlight brightness if the backlight is
  // turned off.
  if (state_ == BACKLIGHT_IDLE_OFF || IsBacklightActiveOff())
    return;

  percent = std::max(0.0, percent);
  als_offset_percent_ = percent;
  has_seen_als_event_ = true;

  // Force a backlight refresh immediately after returning from dim or idle.
  if (als_temporal_state_ == ALS_HYST_IMMEDIATE) {
    als_temporal_state_ = ALS_HYST_IDLE;
    als_adjustment_count_++;
    LOG(INFO) << "Immediate ALS-triggered brightness adjustment.";
    AppendAlsResponse(-1);
    WriteBrightness(false, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_GRADUAL);
    return;
  }

  AppendAlsResponse(lround(percent));

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
    als_temporal_count_ = 0;
  }
  if (als_temporal_count_ >= kAlsHystResponse) {
    als_temporal_count_ = 0;
    als_adjustment_count_++;
    LOG(INFO) << "Ambient light sensor-triggered brightness adjustment.";
    DumpAlsResponses();
    // ALS adjustment should not change brightness offset.
    WriteBrightness(false, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_GRADUAL);
  }
}

bool BacklightController::IsBacklightActiveOff() {
  return state_ == BACKLIGHT_ACTIVE && target_percent_ == 0;
}

double BacklightController::LevelToPercent(int64 raw_level) {
  return kMaxPercent *
      pow(static_cast<double>(raw_level) / max_level_,
          level_to_percent_exponent_);
}

int64 BacklightController::PercentToLevel(double percent) {
  return lround(pow(percent / kMaxPercent, 1.0 / level_to_percent_exponent_) *
                max_level_);
}

void BacklightController::OnBacklightDeviceChanged() {
  LOG(INFO) << "Backlight device changed; reinitializing controller";
  if (Init())
    WriteBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_GRADUAL);
}

double BacklightController::ClampPercentToVisibleRange(double percent) {
  return std::min(kMaxPercent,
                  std::max(LevelToPercent(min_visible_level_), percent));
}

void BacklightController::ReadPrefs() {
  if (!prefs_->GetInt64(kMinVisibleBacklightLevel, &min_visible_level_))
    min_visible_level_ = 1;
  min_visible_level_ = std::max(
      static_cast<int64>(
          lround(kDefaultMinVisibleBrightnessFraction * max_level_)),
      min_visible_level_);
  CHECK_GT(min_visible_level_, 0);
  min_visible_level_ = std::min(min_visible_level_, max_level_);

  CHECK(prefs_->GetDouble(kPluggedBrightnessOffset, &plugged_offset_percent_));
  CHECK(prefs_->GetDouble(kUnpluggedBrightnessOffset,
                          &unplugged_offset_percent_));
  CHECK_GE(plugged_offset_percent_, -kMaxPercent);
  CHECK_LE(plugged_offset_percent_, kMaxPercent);
  CHECK_GE(unplugged_offset_percent_, -kMaxPercent);
  CHECK_LE(unplugged_offset_percent_, kMaxPercent);

  double min_percent = LevelToPercent(min_visible_level_);
  plugged_offset_percent_ = std::max(min_percent, plugged_offset_percent_);
  unplugged_offset_percent_ = std::max(min_percent, unplugged_offset_percent_);
}

void BacklightController::WritePrefs() {
  if (!is_initialized_)
    return;
  if (plugged_state_ == kPowerConnected)
    prefs_->SetDouble(kPluggedBrightnessOffset, plugged_offset_percent_);
  else if (plugged_state_ == kPowerDisconnected)
    prefs_->SetDouble(kUnpluggedBrightnessOffset, unplugged_offset_percent_);
}

bool BacklightController::WriteBrightness(bool adjust_brightness_offset,
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
      observer_->OnScreenBrightnessChanged(target_percent_, cause);
  }

  return true;
}

bool BacklightController::SetBrightness(int64 target_level,
                                        TransitionStyle style) {
  int64 current_level = 0;
  backlight_->GetCurrentBrightnessLevel(&current_level);
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

  if (style == TRANSITION_INSTANT) {
    SetBrightnessHard(target_level, target_level);
    return true;
  }

  // else if (style == TRANSITION_GRADUAL)
  // The following check will require this code to be updated should more styles
  // be added in the future.
  DCHECK_EQ(style, TRANSITION_GRADUAL);

  // We don't want to take more steps than there are adjustment levels between
  // the start brightness and the end brightness.
  int64 num_steps = std::min(abs(diff), kBacklightAnimationFrames);
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
  CHECK(gradual_transition_event_id_ > 0);
  return true;
}

gboolean BacklightController::SetBrightnessStep() {
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

void BacklightController::SetBrightnessHard(int64 level, int64 target_level) {
  if (level != 0 && target_level != 0 && monitor_reconfigure_ &&
      monitor_reconfigure_->HasInternalPanelConnection())
    monitor_reconfigure_->SetInternalPanelOn();

  DLOG(INFO) << "Setting brightness to " << level;
  if (!backlight_->SetBrightnessLevel(level))
    DLOG(INFO) << "Could not set brightness to " << level;

  if (level == 0 && target_level == 0 && monitor_reconfigure_) {
    // If it is in IDLE_OFF state, we call SetScreenOff() to turn off all the
    // display outputs. We don't call SetScreenOff() if it is in SUSPENDED
    // state since kernel driver will turn off the screen.
    if (state_ == BACKLIGHT_IDLE_OFF)
      monitor_reconfigure_->SetScreenOff();
    // If backlight is 0 but we are in ACTIVE state, we turn off the internal
    // panel only.
    else if (state_ == BACKLIGHT_ACTIVE &&
             monitor_reconfigure_->HasInternalPanelConnection())
      monitor_reconfigure_->SetInternalPanelOff();
  }
}

void BacklightController::AppendAlsResponse(int val) {
  als_response_index_ = (als_response_index_ + 1) % arraysize(als_responses_);
  als_responses_[als_response_index_] = val;
}

void BacklightController::DumpAlsResponses() {
  std::string buffer;
  int response_index = als_response_index_;

  for (unsigned int i = 0; i < arraysize(als_responses_); i++) {
    if (!buffer.empty())
      buffer += ", ";
    buffer += StringPrintf("%d", als_responses_[response_index]);
    response_index--;
    if (response_index < 0)
      response_index = arraysize(als_responses_) - 1;
  }

  LOG(INFO) << "ALS history (most recent first): " << buffer;
}

}  // namespace power_manager
