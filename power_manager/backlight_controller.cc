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
const double kIdleBrightness = 10;

// Minimum allowed brightness during startup.
const double kMinInitialBrightness = 10;

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
const int kAlsHystLevel = 5;

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
      als_brightness_level_(0),
      als_hysteresis_level_(0),
      als_temporal_state_(ALS_HYST_IMMEDIATE),
      plugged_brightness_offset_(-1),
      unplugged_brightness_offset_(-1),
      brightness_offset_(NULL),
      state_(BACKLIGHT_UNINITIALIZED),
      plugged_state_(kPowerUnknown),
      local_brightness_(0),
      min_(0),
      max_(-1),
      min_percent_(0.),
      max_percent_(100.),
      num_steps_(kMaxBrightnessSteps),
      is_initialized_(false),
      is_in_transition_(false) {}

bool BacklightController::Init() {
  int64 level;
  if (backlight_->GetBrightness(&level, &max_)) {
    ReadPrefs();
    is_initialized_ = true;
    local_brightness_ = RawBrightnessToLocalBrightness(level);

    // If there are fewer steps than the max, adjust for it.
    // TODO(sque): this is not ideal for some cases, such as max=17, where the
    // steps will have a large spread.  Something we can work on in the future.
    num_steps_ = std::max(static_cast<int64>(1),
                          std::min(kMaxBrightnessSteps, max_));

    // Make sure the min-max brightness range is valid.
    CHECK(max_percent_ - min_percent_ > 0);
    return true;
  }
  return false;
}

bool BacklightController::GetCurrentBrightness(double* level) {
  CHECK(level);
  int64 raw_level;
  if (!backlight_->GetBrightness(&raw_level, &max_))
    return false;

  *level = RawBrightnessToLocalBrightness(raw_level);
  return true;
}

bool BacklightController::GetTargetBrightness(double* level) {
  CHECK(level);
  *level = RawBrightnessToLocalBrightness(target_raw_brightness_);
  return true;
}

bool BacklightController::GetBrightnessScaleLevel(double *level) {
  CHECK(level);
  double brightness;
  GetTargetBrightness(&brightness);
  *level = (brightness - min_percent_) / (max_percent_ - min_percent_) * 100.;
  return true;
}

void BacklightController::IncreaseBrightness() {
  if (!IsInitialized())
    return;

  // Determine the adjustment step size.
  double step_size = (max_percent_ - min_percent_) / num_steps_;
  double new_brightness = ClampToMin(local_brightness_ + step_size);

  if (new_brightness != local_brightness_) {
    // Allow large swing in |brightness_offset_| for absolute brightness
    // outside of clamped brightness region.
    double absolute_brightness = als_brightness_level_ + *brightness_offset_;
    *brightness_offset_ += new_brightness - absolute_brightness;
    WriteBrightness(true);
  }
}

void BacklightController::DecreaseBrightness(bool allow_off) {
  if (!IsInitialized())
    return;

  // Determine the adjustment step size.
  double step_size = (max_percent_ - min_percent_) / num_steps_;
  double new_brightness = ClampToMin(local_brightness_ - step_size);

  if ((new_brightness == min_percent_ && min_percent_ > 0) ||
      new_brightness != local_brightness_) {
    // Set backlight to zero if there is no change in the brightness, but
    // already at a nonzero minimum. (Can go one step lower to zero.)
    if (allow_off && (new_brightness == 0 ||
                      (new_brightness == min_percent_ && min_percent_ > 0))) {
      // Explicitly et new brightness to zero in case backlight was adjusted
      // from min -> 0.
      new_brightness = 0;
    }

    // Allow large swing in |brightness_offset_| for absolute brightness
    // outside of clamped brightness region.
    double absolute_brightness = als_brightness_level_ + *brightness_offset_;
    *brightness_offset_ += new_brightness - absolute_brightness;
    WriteBrightness(true);
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
  WriteBrightness(true);

  // Do not go to dim if backlight is already dimmed.
  if (new_state == BACKLIGHT_DIM &&
      local_brightness_ < ClampToMin(kIdleBrightness))
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
  }
  return true;
}

bool BacklightController::OnPlugEvent(bool is_plugged) {
  if ((brightness_offset_ && is_plugged == plugged_state_) || !is_initialized_)
    return false;
  if (is_plugged) {
    brightness_offset_ = &plugged_brightness_offset_;
    plugged_state_ = kPowerConnected;
    // If unplugged brightness is set to greater than plugged brightness,
    // increase the plugged brightness so that it is not less than unplugged
    // brightness.  Otherwise there will be an unnatural decrease in brightness
    // when the user switches from battery to AC power.
    // If the backlight is in active-but-off state, plugging in AC power
    // shouldn't exit the state.  The plugged brightness should be set to off as
    // well.
    if (IsBacklightActiveOff() ||
        unplugged_brightness_offset_ > plugged_brightness_offset_)
      plugged_brightness_offset_ = unplugged_brightness_offset_;
  } else {
    brightness_offset_ = &unplugged_brightness_offset_;
    plugged_state_ = kPowerDisconnected;
    // If plugged brightness is set to less than unplugged brightness, reduce
    // the unplugged brightness so that it is not greater than plugged
    // brightness.  Otherwise there will be an unnatural increase in brightness
    // when the user switches from AC to battery power.
    if (plugged_brightness_offset_ < unplugged_brightness_offset_)
      unplugged_brightness_offset_ = plugged_brightness_offset_;
  }

  // Adjust new offset to make sure the plug/unplug transition doesn't turn off
  // the screen.  If the backlight is in active-but-off state, don't make this
  // adjustment.
  if (!IsBacklightActiveOff() &&
      *brightness_offset_ + als_brightness_level_ < 1)
    *brightness_offset_ = 1 - als_brightness_level_;

  return WriteBrightness(true);
}

void BacklightController::SetAlsBrightnessLevel(int64 level) {
  if (!is_initialized_)
    return;

  // Do not use ALS to adjust the backlight brightness if the backlight is
  // turned off.
  if (state_ == BACKLIGHT_IDLE_OFF || IsBacklightActiveOff())
    return;

  als_brightness_level_ = level;

  // Force a backlight refresh immediately after returning from dim or idle.
  if (als_temporal_state_ == ALS_HYST_IMMEDIATE) {
    als_temporal_state_ = ALS_HYST_IDLE;
    LOG(INFO) << "Ambient light sensor-triggered brightness adjustment.";
    WriteBrightness(false);
    return;
  }

  // Apply level and temporal hysteresis to light sensor readings to reduce
  // backlight changes caused by minor and transient ambient light changes.
  int64 diff = level - als_hysteresis_level_;
  AlsHysteresisState new_state;

  if (diff < -kAlsHystLevel) {
    new_state = ALS_HYST_DOWN;
  } else if (diff > kAlsHystLevel) {
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
    LOG(INFO) << "Ambient light sensor-triggered brightness adjustment.";
    WriteBrightness(false);  // ALS adjustment should not change brightness
                             // offset.
  }
}

void BacklightController::SetMinimumBrightness(int64 level) {
  min_percent_ = level;
  min_ = LocalBrightnessToRawBrightness(level);
}

bool BacklightController::IsBacklightActiveOff() {
  return state_ == BACKLIGHT_ACTIVE && local_brightness_ == 0;
}

double BacklightController::Clamp(double value) {
  return std::min(max_percent_, std::max(0., value));
}

double BacklightController::ClampToMin(double value) {
  return std::min(max_percent_, std::max(min_percent_, value));
}

double BacklightController::RawBrightnessToLocalBrightness(int64 raw_level) {
  return max_percent_ * raw_level / max_;
}

int64 BacklightController::LocalBrightnessToRawBrightness(double local_level) {
  return lround(local_level * max_ / max_percent_);
}

void BacklightController::ReadPrefs() {
  CHECK(prefs_->GetDouble(kPluggedBrightnessOffset,
                          &plugged_brightness_offset_));
  CHECK(prefs_->GetDouble(kUnpluggedBrightnessOffset,
                         &unplugged_brightness_offset_));
  CHECK(plugged_brightness_offset_ >= -max_percent_);
  CHECK(plugged_brightness_offset_ <= max_percent_);
  CHECK(unplugged_brightness_offset_ >= -max_percent_);
  CHECK(unplugged_brightness_offset_ <= max_percent_);

  // Adjust brightness offset values to make sure that the backlight is not
  // initially set to too low of a level.
  double minimum_starting_brightness = std::max(kMinInitialBrightness,
                                                min_percent_);
  if (als_brightness_level_ + plugged_brightness_offset_ <
      minimum_starting_brightness)
    plugged_brightness_offset_ = minimum_starting_brightness -
                                                  als_brightness_level_;
  if (als_brightness_level_ + unplugged_brightness_offset_ <
      minimum_starting_brightness)
    unplugged_brightness_offset_ = minimum_starting_brightness -
                                                  als_brightness_level_;
}

void BacklightController::WritePrefs() {
  if (!is_initialized_)
    return;
  // Do not store brightness that falls below a particular threshold, so that
  // when powerd restarts, the screen does not appear to be off.
  if (plugged_state_ == kPowerConnected)
    prefs_->SetDouble(kPluggedBrightnessOffset, plugged_brightness_offset_);
  else if (plugged_state_ == kPowerDisconnected)
    prefs_->SetDouble(kUnpluggedBrightnessOffset, unplugged_brightness_offset_);
}

bool BacklightController::IsInitialized() {
  // brightness_offset_ will be initialized once polling of plugged event has
  // occurred once
  return (is_initialized_ && brightness_offset_);
}

bool BacklightController::WriteBrightness(bool adjust_brightness_offset) {
  if (!IsInitialized())
    return false;

  double old_brightness = local_brightness_;
  if (state_ == BACKLIGHT_ACTIVE || state_ == BACKLIGHT_ALREADY_DIMMED) {
    local_brightness_ = ClampToMin(als_brightness_level_ + *brightness_offset_);
    // Do not turn off backlight if this is a "soft" adjustment -- e.g. due to
    // ALS change.
    // Also, do not turn off the backlight if it has been dimmed and idled.
    if (!adjust_brightness_offset || state_ == BACKLIGHT_ALREADY_DIMMED) {
      if (local_brightness_ == 0 && old_brightness > 0)
        local_brightness_ = 1;
      else if (local_brightness_ > 0 && old_brightness == 0)
        local_brightness_ = 0;
    }
    // Adjust offset in case brightness was changed.
    if (adjust_brightness_offset)
      *brightness_offset_ = local_brightness_ - als_brightness_level_;
  } else if (state_ == BACKLIGHT_DIM) {
    // When in dimmed state, set to dim level only if it results in a reduction
    // of system brightness.  Also, make sure idle brightness is not lower than
    // the minimum brightness.
    if (old_brightness > ClampToMin(kIdleBrightness)) {
      local_brightness_ = ClampToMin(kIdleBrightness);
    } else {
      LOG(INFO) << "Not dimming because backlight is already dim.";
      // Even if the brightness is below the dim level, make sure it is not
      // below the minimum brightness.
      local_brightness_ = ClampToMin(local_brightness_);
    }
  } else if (state_ == BACKLIGHT_IDLE_OFF || state_ == BACKLIGHT_SUSPENDED) {
    local_brightness_ = 0;
  }
  als_hysteresis_level_ = als_brightness_level_;
  int64 val = LocalBrightnessToRawBrightness(local_brightness_);
  LOG(INFO) << "WriteBrightness: " << old_brightness << "% -> "
            << local_brightness_ << "%";
  if (SetBrightnessGradual(val))
    WritePrefs();
  return local_brightness_ != old_brightness;
}

bool BacklightController::SetBrightnessGradual(int64 target_level) {
  LOG(INFO) << "Attempting to set brightness to " << target_level;
  int64 current_level, max_level;
  backlight_->GetBrightness(&current_level, &max_level);
  LOG(INFO) << "Current actual brightness: " << current_level;
  LOG(INFO) << "Current target brightness: " << target_raw_brightness_;

  // If this is a redundant call (existing target level is the same as
  // new target level), ignore this call.
  if (target_raw_brightness_ == target_level)
    return true;

  // Otherwise, set to the new target brightness. This will disable any
  // outstanding brightness transition to a different brightness.
  target_raw_brightness_ = target_level;
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
  if (target_raw_brightness_ != target_level)
    return false; // Return false so glib doesn't repeat.

  DLOG(INFO) << "Setting brightness to " << level;
  if (!backlight_->SetBrightness(level))
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
