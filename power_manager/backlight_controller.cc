// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/backlight_controller.h"

#include <cmath>
#include <gdk/gdkx.h>
#include <X11/extensions/dpms.h>

#include "base/logging.h"
#include "power_manager/ambient_light_sensor.h"
#include "power_manager/power_constants.h"
#include "power_manager/power_prefs_interface.h"

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

// Maximum number of brightness adjustment steps.
const int64 kMaxBrightnessSteps = 16;

// Number of light sensor samples required to overcome temporal hysteresis.
const int kAlsHystSamples = 4;

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
      step_percent_(1.0),
      idle_brightness_percent_(kIdleBrightnessFraction * 100.0),
      level_to_percent_exponent_(kDefaultLevelToPercentExponent),
      is_initialized_(false),
      target_level_(0),
      is_in_transition_(false) {
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
#ifdef IS_DESKTOP
  state_ = new_state;
#else
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
    // If returning from suspend, force the backlight to zero to cancel out any
    // kernel driver behavior that sets it to some other value.  This allows
    // backlight controller to restore the backlight value from 0 to the saved
    // active level.
    if (old_state == BACKLIGHT_SUSPENDED)
      style = TRANSITION_INSTANT;
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
#endif // defined(IS_DESKTOP)

  LOG(INFO) << PowerStateToString(old_state) << " -> "
            << PowerStateToString(new_state);

  if (GDK_DISPLAY() == NULL)
    return true;
  if (!DPMSCapable(GDK_DISPLAY())) {
    LOG(WARNING) << "X Server is not DPMS capable";
  } else {
    CHECK(DPMSEnable(GDK_DISPLAY()));
    if (new_state == BACKLIGHT_ACTIVE)
      CHECK(DPMSForceLevel(GDK_DISPLAY(), DPMSModeOn));
#ifdef IS_DESKTOP
    // TODO(sque): will be deprecated once we have external display backlight
    // control.  chrome-os-partner:6320
    else if (new_state == BACKLIGHT_IDLE_OFF)
      CHECK(DPMSForceLevel(GDK_DISPLAY(), DPMSModeOff));
#endif // defined(IS_DESKTOP)
  }
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
    LOG(INFO) << "Ambient light sensor-triggered brightness adjustment.";
    WriteBrightness(false, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_GRADUAL);
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
    als_temporal_count_ = 0;
  }
  if (als_temporal_count_ >= kAlsHystSamples) {
    als_temporal_count_ = 0;
    als_adjustment_count_++;
    LOG(INFO) << "Ambient light sensor-triggered brightness adjustment.";
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
  is_in_transition_ = true;
  int64 previous_level = current_level;
  for (int i = 0; i < kBacklightAnimationFrames; ++i) {
    int64 step_level =
        current_level + diff * (i + 1) / kBacklightAnimationFrames;
    if (step_level == previous_level)
      continue;
    g_timeout_add(i * kBacklightAnimationMs, SetBrightnessHardThunk,
                  CreateSetBrightnessHardArgs(this, step_level, target_level));
    previous_level = step_level;
  }
  return true;
}

gboolean BacklightController::SetBrightnessHard(int64 level,
                                                int64 target_level) {
  // If the target brightness of this call does not match the backlight's
  // current target brightness, it must be from an earlier backlight adjustment
  // that had a different target brightness.  In that case, it is invalidated
  // so do nothing.
  if (target_level_ != target_level)
    return false; // Return false so glib doesn't repeat.

  DLOG(INFO) << "Setting brightness to " << level;
  if (!backlight_->SetBrightnessLevel(level))
    DLOG(INFO) << "Could not set brightness to " << level;

  if (level == target_level)
    is_in_transition_ = false;

  // Turn off screen if transitioning to zero.
  if (level == 0 && target_level == 0 && GDK_DISPLAY() != NULL &&
      DPMSCapable(GDK_DISPLAY()) && state_ == BACKLIGHT_IDLE_OFF)
    CHECK(DPMSForceLevel(GDK_DISPLAY(), DPMSModeOff));
  return false; // Return false so glib doesn't repeat.
}

}  // namespace power_manager
