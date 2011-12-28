// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/backlight_controller.h"

#include <cmath>
#include <gdk/gdkx.h>
#include <X11/extensions/dpms.h>

#include "base/logging.h"
#include "power_manager/ambient_light_sensor.h"
#include "power_manager/backlight_interface.h"
#include "power_manager/power_constants.h"
#include "power_manager/power_prefs_interface.h"

namespace {

// Set brightness to this value when going into idle-induced dim state.
const double kIdleBrightnessPercent = 10;

// Minimum allowed brightness during startup.
const double kMinInitialBrightnessPercent = 10;

// Gradually change backlight level to new brightness by breaking up the
// transition into N steps, where N = kBacklightNumSteps.
const int kBacklightNumSteps = 8;

// Time between backlight adjustment steps, in milliseconds.
const int kBacklightStepTimeMS = 30;

// Maximum number of brightness adjustment steps.
const int64 kMaxBrightnessSteps = 16;

// Number of light sensor samples required to overcome temporal hysteresis.
const int kAlsHystSamples = 4;

// Backlight change (in %) required to overcome light sensor level hysteresis.
const int kAlsHystPercent = 5;

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
      als_offset_percent_(0),
      als_hysteresis_percent_(0),
      als_temporal_state_(ALS_HYST_IMMEDIATE),
      als_adjustment_count_(0),
      user_adjustment_count_(0),
      plugged_offset_percent_(-1),
      unplugged_offset_percent_(-1),
      current_offset_percent_(NULL),
      state_(BACKLIGHT_UNINITIALIZED),
      plugged_state_(kPowerUnknown),
      target_percent_(0),
      min_level_(0),
      max_level_(-1),
      min_percent_(0.),
      max_percent_(100.),
      num_steps_(kMaxBrightnessSteps),
      is_initialized_(false),
      target_level_(0),
      is_in_transition_(false) {}

bool BacklightController::Init() {
  if (!backlight_->GetMaxBrightnessLevel(&max_level_) ||
      !backlight_->GetCurrentBrightnessLevel(&target_level_))
    return false;

  ReadPrefs();
  is_initialized_ = true;
  target_percent_ = LevelToPercent(target_level_);

  // If there are fewer steps than the max, adjust for it.
  // TODO(sque): this is not ideal for some cases, such as max=17, where the
  // steps will have a large spread.  Something we can work on in the future.
  num_steps_ = std::max(static_cast<int64>(1),
                        std::min(kMaxBrightnessSteps, max_level_));

  // Make sure the min-max brightness range is valid.
  CHECK(max_percent_ - min_percent_ > 0);
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

void BacklightController::IncreaseBrightness(BrightnessChangeCause cause) {
  if (!IsInitialized())
    return;
  int64 current_level;
  backlight_->GetCurrentBrightnessLevel(&current_level);
  // Determine the adjustment step size.
  double step_size = (max_percent_ - min_percent_) / num_steps_;
  double new_percent = ClampToMin(target_percent_ + step_size);

  if (new_percent != target_percent_) {
    // Allow large swing in |current_offset_percent_| for absolute brightness
    // outside of clamped brightness region.
    double absolute_percent = als_offset_percent_ + *current_offset_percent_;
    *current_offset_percent_ += new_percent - absolute_percent;
    WriteBrightness(true, cause);
  }
}

void BacklightController::DecreaseBrightness(bool allow_off,
                                             BrightnessChangeCause cause) {
  if (!IsInitialized())
    return;

  int64 current_level;
  backlight_->GetCurrentBrightnessLevel(&current_level);
  double step_size = (max_percent_ - min_percent_) / num_steps_;

  // Lower backlight to the next step, turn it off if already at the minimum.
  double new_percent;
  if (target_percent_ > min_percent_)
    new_percent = ClampToMin(target_percent_ - step_size);
  else
    new_percent = 0;

  if (new_percent != target_percent_ && (allow_off || new_percent > 0)) {
    // Allow large swing in |current_offset_percent_| for absolute brightness
    // outside of clamped brightness region.
    double absolute_percent = als_offset_percent_ + *current_offset_percent_;
    *current_offset_percent_ += new_percent - absolute_percent;
    WriteBrightness(true, cause);
  }
}

bool BacklightController::SetPowerState(PowerState new_state) {
  PowerState old_state = state_;
#ifdef IS_DESKTOP
  state_ = new_state;
#else
  if (new_state == state_ || !is_initialized_)
    return false;
  CHECK(new_state != BACKLIGHT_UNINITIALIZED);

  // If backlight is turned off, do not transition to dim or off states.
  // From ACTIVE_OFF state only transition to ACTIVE and SUSPEND states.
  if (IsBacklightActiveOff() && (new_state == BACKLIGHT_IDLE_OFF ||
                                 new_state == BACKLIGHT_DIM ||
                                 new_state == BACKLIGHT_ALREADY_DIMMED))
    return false;

  state_ = new_state;

#ifdef HAS_ALS
  // For the first time SetPowerState() is called (when the system just
  // boots), if the ALS value has not been seen, skip the backlight
  // adjustment.
  if (old_state == BACKLIGHT_UNINITIALIZED) {
    if (has_seen_als_event_) {
      WriteBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED);
    } else {
      LOG(INFO) << "First time SetPowerState() called, skip the backlight "
                << "brightness adjustment since no ALS value available yet.";
    }
  } else {
    WriteBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED);
  }
#else
  WriteBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED);
#endif // defined(HAS_ALS)

  // Do not go to dim if backlight is already dimmed.
  if (new_state == BACKLIGHT_DIM &&
      target_percent_ < ClampToMin(kIdleBrightnessPercent))
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
  if ((current_offset_percent_ && is_plugged == plugged_state_) ||
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
      return WriteBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED);
    }
    LOG(INFO) << "First time OnPlugEvent() called, skip the backlight "
              << "brightness adjustment since no ALS value available yet.";
    return true;
  }
#endif // defined(HAS_ALS)
  return WriteBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED);
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

  als_offset_percent_ = percent;
  has_seen_als_event_ = true;

  // Force a backlight refresh immediately after returning from dim or idle.
  if (als_temporal_state_ == ALS_HYST_IMMEDIATE) {
    als_temporal_state_ = ALS_HYST_IDLE;
    als_adjustment_count_++;
    LOG(INFO) << "Ambient light sensor-triggered brightness adjustment.";
    WriteBrightness(false, BRIGHTNESS_CHANGE_AUTOMATED);
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
    WriteBrightness(false, BRIGHTNESS_CHANGE_AUTOMATED);
  }
}

void BacklightController::SetMinimumBrightnessPercent(double percent) {
  min_percent_ = percent;
  min_level_ = PercentToLevel(percent);
}

bool BacklightController::IsBacklightActiveOff() {
  return state_ == BACKLIGHT_ACTIVE && target_percent_ == 0;
}

double BacklightController::Clamp(double percent) {
  return std::min(max_percent_, std::max(0., percent));
}

double BacklightController::ClampToMin(double percent) {
  return std::min(max_percent_, std::max(min_percent_, percent));
}

double BacklightController::LevelToPercent(int64 raw_level) {
  return max_percent_ * raw_level / max_level_;
}

int64 BacklightController::PercentToLevel(double local_percent) {
  return lround(local_percent * max_level_ / max_percent_);
}

void BacklightController::ReadPrefs() {
  CHECK(prefs_->GetDouble(kPluggedBrightnessOffset, &plugged_offset_percent_));
  CHECK(prefs_->GetDouble(kUnpluggedBrightnessOffset,
                         &unplugged_offset_percent_));
  CHECK(plugged_offset_percent_ >= -max_percent_);
  CHECK(plugged_offset_percent_ <= max_percent_);
  CHECK(unplugged_offset_percent_ >= -max_percent_);
  CHECK(unplugged_offset_percent_ <= max_percent_);

  // Adjust brightness offset values to make sure that the backlight is not
  // initially set to too low of a level.
  double min_starting_brightness =
      std::max(kMinInitialBrightnessPercent, min_percent_);
  if (als_offset_percent_ + plugged_offset_percent_ < min_starting_brightness)
    plugged_offset_percent_ = min_starting_brightness - als_offset_percent_;
  if (als_offset_percent_ + unplugged_offset_percent_ <
      min_starting_brightness)
    unplugged_offset_percent_ = min_starting_brightness - als_offset_percent_;
}

void BacklightController::WritePrefs() {
  if (!is_initialized_)
    return;
  // Do not store brightness that falls below a particular threshold, so that
  // when powerd restarts, the screen does not appear to be off.
  if (plugged_state_ == kPowerConnected)
    prefs_->SetDouble(kPluggedBrightnessOffset, plugged_offset_percent_);
  else if (plugged_state_ == kPowerDisconnected)
    prefs_->SetDouble(kUnpluggedBrightnessOffset, unplugged_offset_percent_);
}

bool BacklightController::IsInitialized() {
  // |current_offset_percent_| will be initialized once polling of plugged
  // event has occurred once.
  return (is_initialized_ && current_offset_percent_);
}

bool BacklightController::WriteBrightness(bool adjust_brightness_offset,
                                          BrightnessChangeCause cause) {
  if (!IsInitialized())
    return false;

  if (cause == BRIGHTNESS_CHANGE_USER_INITIATED)
    user_adjustment_count_++;

  double old_percent = target_percent_;
  if (state_ == BACKLIGHT_ACTIVE || state_ == BACKLIGHT_ALREADY_DIMMED) {
    target_percent_ =
        ClampToMin(als_offset_percent_ + *current_offset_percent_);
    // Do not turn off backlight if this is a "soft" adjustment -- e.g. due to
    // ALS change.
    // Also, do not turn off the backlight if it has been dimmed and idled.
    if (!adjust_brightness_offset || state_ == BACKLIGHT_ALREADY_DIMMED) {
      if (target_percent_ == 0 && old_percent > 0)
        target_percent_ = 1;
      else if (target_percent_ > 0 && old_percent == 0)
        target_percent_ = 0;
    }
    // Adjust offset in case brightness was changed.
    if (adjust_brightness_offset)
      *current_offset_percent_ = target_percent_ - als_offset_percent_;
  } else if (state_ == BACKLIGHT_DIM) {
    // When in dimmed state, set to dim level only if it results in a reduction
    // of system brightness.  Also, make sure idle brightness is not lower than
    // the minimum brightness.
    if (old_percent > ClampToMin(kIdleBrightnessPercent)) {
      target_percent_ = ClampToMin(kIdleBrightnessPercent);
    } else {
      LOG(INFO) << "Not dimming because backlight is already dim.";
      // Even if the brightness is below the dim level, make sure it is not
      // below the minimum brightness.
      target_percent_ = ClampToMin(target_percent_);
    }
  } else if (state_ == BACKLIGHT_IDLE_OFF || state_ == BACKLIGHT_SUSPENDED) {
    target_percent_ = 0;
  }

  als_hysteresis_percent_ = als_offset_percent_;
  int64 level = PercentToLevel(target_percent_);
  LOG(INFO) << "WriteBrightness: " << old_percent << "% -> "
            << target_percent_ << "%";
  if (SetBrightnessGradual(level)) {
    WritePrefs();
    if (observer_)
      observer_->OnBrightnessChanged(target_percent_, cause);
  }

  return target_percent_ != old_percent;
}

bool BacklightController::SetBrightnessGradual(int64 target_level) {
  LOG(INFO) << "Attempting to set brightness to " << target_level;
  int64 current_level;
  backlight_->GetCurrentBrightnessLevel(&current_level);
  LOG(INFO) << "Current actual brightness: " << current_level;
  LOG(INFO) << "Current target brightness: " << target_level_;

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

  LOG(INFO) << "Setting to new target brightness " << target_level;
  is_in_transition_ = true;
  int64 previous_level = current_level;
  for (int i = 0; i < kBacklightNumSteps; ++i) {
    int64 step_level = current_level + diff * (i + 1) / kBacklightNumSteps;
    if (step_level == previous_level)
      continue;
    g_timeout_add(i * kBacklightStepTimeMS, SetBrightnessHardThunk,
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
