// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/backlight_controller.h"

#include <gdk/gdkx.h>
#include <math.h>
#include <X11/extensions/dpms.h>

#include "base/logging.h"
#include "power_manager/ambient_light_sensor.h"
#include "power_manager/power_constants.h"

namespace power_manager {

// Set brightness to this value when going into idle-induced dim state.
static const int64 kIdleBrightness = 10;

// Minimum allowed brightness during startup.
static const int64 kMinInitialBrightness = 10;

// When waiting for the backlight to be turned off before turning the display,
// wait for this time between polling the backlight value.
static const int64 kDisplayOffDelayMS = 100;

// Gradually change backlight level to new brightness by breaking up the
// transition into N steps, where N = kBacklightNumSteps.
const int kBacklightNumSteps = 8;

// Time between backlight adjustment steps, in milliseconds.
const int kBacklightStepTimeMS = 30;

// String names for power states.
static const char* PowerStateToString(PowerState state) {
  switch (state) {
    case BACKLIGHT_ACTIVE_ON:
      return "state(ACTIVE_ON)";
    case BACKLIGHT_DIM:
      return "state(DIM)";
    case BACKLIGHT_IDLE_OFF:
      return "state(IDLE_OFF)";
    case BACKLIGHT_ACTIVE_OFF:
      return "state(ACTIVE_OFF)";
    case BACKLIGHT_SUSPENDED:
      return "state(SUSPENDED)";
    case BACKLIGHT_UNINITIALIZED:
      return "state(UNINITIALIZED)";
    default:
      NOTREACHED();
      return "";
  }
}

BacklightController::BacklightController(BacklightInterface* backlight,
                                         PowerPrefsInterface* prefs)
    : backlight_(backlight),
      prefs_(prefs),
      light_sensor_(NULL),
      als_brightness_level_(0),
      als_hysteresis_level_(0),
      plugged_brightness_offset_(-1),
      unplugged_brightness_offset_(-1),
      brightness_offset_(NULL),
      state_(BACKLIGHT_UNINITIALIZED),
      plugged_state_(kPowerUnknown),
      local_brightness_(0),
      min_(0),
      max_(-1),
      min_percent_(0),
      is_initialized_(false) {}

bool BacklightController::Init() {
  int64 level;
  if (backlight_->GetBrightness(&level, &max_)) {
    ReadPrefs();
    is_initialized_ = true;
    return true;
  }
  return false;
}

bool BacklightController::GetBrightness(int64* level) {
  int64 raw_level;
  if (!backlight_->GetBrightness(&raw_level, &max_))
    return false;

  *level = RawBrightnessToLocalBrightness(raw_level);
  return true;
}

bool BacklightController::GetTargetBrightness(int64* level) {
  *level = RawBrightnessToLocalBrightness(target_raw_brightness_);
  return true;
}

void BacklightController::IncreaseBrightness() {
  if (!is_initialized_)
    return;
  if (ReadBrightness()) {
    // Give the user between 8 and 16 distinct brightness levels
    int64 offset = 1 + (max_ >> 4);
    int64 new_val = offset + LocalBrightnessToRawBrightness(local_brightness_);
    int64 new_brightness = ClampToMin(RawBrightnessToLocalBrightness(new_val));
    if (new_brightness != local_brightness_) {
      SetPowerState(BACKLIGHT_ACTIVE_ON);
      // Allow large swing in brightness_offset for absolute brightness
      // outside of clamped brightness region.
      int64 absolute_brightness = als_brightness_level_ + *brightness_offset_;
      *brightness_offset_ += new_brightness - absolute_brightness;
      WriteBrightness();
    }
  }
}

void BacklightController::DecreaseBrightness() {
  if (!is_initialized_)
    return;
  if (ReadBrightness()) {
    // Give the user between 8 and 16 distinct brightness levels
    int64 offset = 1 + (max_ >> 4);
    int64 new_val = LocalBrightnessToRawBrightness(local_brightness_) - offset;
    int64 new_brightness = ClampToMin(RawBrightnessToLocalBrightness(new_val));
    if (new_brightness != local_brightness_ || new_brightness == min_percent_) {
      if (new_brightness == min_percent_)
        SetPowerState(BACKLIGHT_ACTIVE_OFF);
      // Allow large swing in brightness_offset for absolute brightness
      // outside of clamped brightness region.
      int64 absolute_brightness = als_brightness_level_ + *brightness_offset_;
      *brightness_offset_ += new_brightness - absolute_brightness;
      WriteBrightness();
    }
  }
}

bool BacklightController::SetPowerState(PowerState state) {
  if (state == state_ || !is_initialized_)
    return false;
  CHECK(state != BACKLIGHT_UNINITIALIZED);

  // If backlight is turned off, do not transition to dim or off states.
  // From ACTIVE_OFF state only transition to ACTIVE_ON and SUSPEND states.
  if (state_ == BACKLIGHT_ACTIVE_OFF && (state == BACKLIGHT_IDLE_OFF ||
                                         state == BACKLIGHT_DIM))
    return false;

  LOG(INFO) << PowerStateToString(state_) << " -> "
            << PowerStateToString(state);
  ReadBrightness();
  state_ = state;
  bool changed_brightness = WriteBrightness();

  if (light_sensor_)
    light_sensor_->EnableOrDisableSensor(state_);

  if (GDK_DISPLAY() == NULL)
    return changed_brightness;
  if (!DPMSCapable(GDK_DISPLAY())) {
    LOG(WARNING) << "X Server is not DPMS capable";
  } else {
    CHECK(DPMSEnable(GDK_DISPLAY()));
    if (state == BACKLIGHT_IDLE_OFF) {
      SetBrightnessToZero();
    } else if (state == BACKLIGHT_ACTIVE_ON) {
      CHECK(DPMSForceLevel(GDK_DISPLAY(), DPMSModeOn));
    }
  }
  return changed_brightness;
}

bool BacklightController::OnPlugEvent(bool is_plugged) {
  if ((brightness_offset_ && is_plugged == plugged_state_) || !is_initialized_)
    return false;
  if (is_plugged) {
    brightness_offset_ = &plugged_brightness_offset_;
    plugged_state_ = kPowerConnected;
  } else {
    brightness_offset_ = &unplugged_brightness_offset_;
    plugged_state_ = kPowerDisconnected;
  }
  return WriteBrightness();
}

void BacklightController::SetMinimumBrightness(int64 level) {
  min_percent_ = level;
}

void BacklightController::SetAlsBrightnessLevel(int64 level) {
  if (!is_initialized_)
    return;

  // Do not use ALS to adjust the backlight brightness if the backlight is
  // turned off.
  if (state_ == BACKLIGHT_ACTIVE_OFF || state_ == BACKLIGHT_IDLE_OFF)
    return;

  als_brightness_level_ = level;

  // Only a change of 5% of the brightness range will force a change.
  int64 diff = level - als_hysteresis_level_;
  if (diff < 0)
    diff = -diff;
  if (diff >= 5)
    WriteBrightness();
}

int64 BacklightController::Clamp(int64 value) {
  return std::min(100LL, std::max(0LL, value));
}

int64 BacklightController::ClampToMin(int64 value) {
  return std::min(100LL, std::max(min_percent_, value));
}

int64 BacklightController::RawBrightnessToLocalBrightness(int64 raw_level) {
  return lround(100. * (raw_level ) / (max_ ));
}

int64 BacklightController::LocalBrightnessToRawBrightness(int64 local_level) {
  return lround((max_ ) * local_level / 100.);
}

void BacklightController::ReadPrefs() {
  CHECK(prefs_->GetInt64(kPluggedBrightnessOffset,
                         &plugged_brightness_offset_));
  CHECK(prefs_->GetInt64(kUnpluggedBrightnessOffset,
                         &unplugged_brightness_offset_));
  CHECK(plugged_brightness_offset_ >= -100);
  CHECK(plugged_brightness_offset_ <= 100);
  CHECK(unplugged_brightness_offset_ >= -100);
  CHECK(unplugged_brightness_offset_ <= 100);

  // Adjust brightness offset values to make sure that the backlight is not
  // initially set to too low of a level.
  int64 minimum_starting_brightness =
      std::max(kMinInitialBrightness, min_percent_);
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
  bool store_plugged_brightness = false;
  bool store_unplugged_brightness = false;
  // Do not store brightness that falls below a particular threshold, so that
  // when powerd restarts, the screen does not appear to be off.
  if (plugged_state_ == kPowerConnected) {
    store_plugged_brightness = true;
    // If plugged brightness is set to less than unplugged brightness, reduce
    // the unplugged brightness so that it is not greater than plugged
    // brightness.  Otherwise there will be an unnatural increase in brightness
    // when the user switches from AC to battery power.
    if (plugged_brightness_offset_ < unplugged_brightness_offset_) {
      unplugged_brightness_offset_ = plugged_brightness_offset_;
      store_unplugged_brightness = true;
    }
  } else if (plugged_state_ == kPowerDisconnected) {
    store_unplugged_brightness = true;
    // If unplugged brightness is set to greater than plugged brightness,
    // increase the plugged brightness so that it is not less than unplugged
    // brightness.  Otherwise there will be an unnatural decrease in brightness
    // when the user switches from battery to AC power.
    if (unplugged_brightness_offset_ > plugged_brightness_offset_) {
      plugged_brightness_offset_ = unplugged_brightness_offset_;
      store_plugged_brightness = true;
    }
  }

  // Store the brightness levels to preference files.
  if (store_plugged_brightness)
    prefs_->SetInt64(kPluggedBrightnessOffset, plugged_brightness_offset_);

  if (store_unplugged_brightness)
    prefs_->SetInt64(kUnpluggedBrightnessOffset, unplugged_brightness_offset_);
}

bool BacklightController::ReadBrightness() {
  if (!is_initialized_)
    return false;
  CHECK(brightness_offset_ != NULL) << "Plugged state must be initialized";
  int64 level = RawBrightnessToLocalBrightness(target_raw_brightness_);
  if (level != local_brightness_) {
    // Another program adjusted the brightness. Sync up.
    // TODO(sque): This is currently useless code due to the implementation of
    // smoothing and target brightness.  However, that will soon change, and
    // this code will become useful again.  Delete the Clamp function as needed.
    LOG(INFO) << "ReadBrightness: " << local_brightness_ << " -> " << level;
    int64 brightness = Clamp(als_brightness_level_ + *brightness_offset_);
    int64 diff = Clamp(brightness + level - local_brightness_) - brightness;
    *brightness_offset_ += diff;
    local_brightness_ = level;
    WritePrefs();
    return false;
  }
  return true;
}

bool BacklightController::WriteBrightness() {
  if (!is_initialized_)
    return false;
  CHECK(brightness_offset_ != NULL) << "Plugged state must be initialized";
  int64 old_brightness = local_brightness_;
  if (state_ == BACKLIGHT_ACTIVE_ON) {
    local_brightness_ = ClampToMin(als_brightness_level_ + *brightness_offset_);
  } else if (state_ == BACKLIGHT_DIM) {
    // When in dimmed state, set to dim level only if it results in a reduction
    // of system brightness.  Also, make sure idle brightness is not lower than
    // the minimum brightness.
    if (local_brightness_ > ClampToMin(kIdleBrightness)) {
      local_brightness_ = ClampToMin(kIdleBrightness);
    } else {
      LOG(INFO) << "Not dimming because backlight is already dim.";
      // Even if the brightness is below the dim level, make sure it is not
      // below the minimum brightness.
      local_brightness_ = ClampToMin(local_brightness_);
    }
  } else if (state_ == BACKLIGHT_IDLE_OFF || state_ == BACKLIGHT_ACTIVE_OFF ||
             state_ == BACKLIGHT_SUSPENDED) {
    local_brightness_ = 0;
  }
  als_hysteresis_level_ = als_brightness_level_;
  int64 val = LocalBrightnessToRawBrightness(local_brightness_);
  local_brightness_ = RawBrightnessToLocalBrightness(val);
  LOG(INFO) << "WriteBrightness: " << old_brightness << " -> "
            << local_brightness_;
  if (SetBrightnessGradual(val))
    WritePrefs();
  return local_brightness_ != old_brightness;
}

void BacklightController::SetBrightnessToZero() {
  if (!is_initialized_)
    return;
  local_brightness_ = 0;
  if (SetBrightnessGradual(0))
    WritePrefs();
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

  // Turn off screen if transitioning to zero.
  if (level == 0 && target_level == 0 && GDK_DISPLAY() != NULL &&
      state_ == BACKLIGHT_IDLE_OFF)
    CHECK(DPMSForceLevel(GDK_DISPLAY(), DPMSModeOff));
  return false; // Return false so glib doesn't repeat.
}

}  // namespace power_manager
