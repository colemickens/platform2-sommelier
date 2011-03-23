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
      system_brightness_(0),
      min_(0),
      max_(-1),
      is_initialized_(false) {}

bool BacklightController::Init() {
  int64 level;
  if (backlight_->GetBrightness(&level, &max_)) {
    ReadPrefs();
    is_initialized_ = true;
    backlight_->SetScreenOffFunc(TurnScreenOffThunk, this);
    return true;
  }
  return false;
}

bool BacklightController::GetBrightness(int64* level) {
  int64 raw_level;
  if (!backlight_->GetBrightness(&raw_level, &max_))
    return false;
  *level = lround(100. * raw_level / max_);
  return true;
}

bool BacklightController::GetTargetBrightness(int64* level) {
  int64 raw_level;
  if (!backlight_->GetTargetBrightness(&raw_level))
    return false;
  *level = lround(100. * raw_level / max_);
  return true;
}

void BacklightController::IncreaseBrightness() {
  if (!is_initialized_)
    return;
  if (ReadBrightness()) {
    // Give the user between 8 and 16 distinct brightness levels
    int64 offset = 1 + (max_ >> 4);
    int64 new_val = offset + lround(max_ * system_brightness_ / 100.);
    int64 new_brightness = Clamp(lround(100. * new_val / max_));
    if (new_brightness != system_brightness_) {
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
    int64 new_val = lround(max_ * system_brightness_ / 100.) - offset;
    int64 new_brightness = Clamp(lround(100. * new_val / max_));
    if (new_brightness != system_brightness_) {
      if (new_brightness == 0)
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

void BacklightController::SetAlsBrightnessLevel(int64 level) {
  if (!is_initialized_)
    return;
  int64 target_level = 0;
  CHECK(GetTargetBrightness(&target_level));
  // Do not use ALS to adjust the backlight brightness if the backlight is
  // turned off.
  if (target_level == 0)
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

void BacklightController::ReadPrefs() {
  CHECK(prefs_->GetInt64(kPluggedBrightnessOffset,
                         &plugged_brightness_offset_));
  CHECK(prefs_->GetInt64(kUnpluggedBrightnessOffset,
                         &unplugged_brightness_offset_));
  CHECK(plugged_brightness_offset_ >= -100);
  CHECK(plugged_brightness_offset_ <= 100);
  CHECK(unplugged_brightness_offset_ >= -100);
  CHECK(unplugged_brightness_offset_ <= 100);
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
  int64 level;
  if (GetTargetBrightness(&level) && level != system_brightness_) {
    // Another program adjusted the brightness. Sync up.
    LOG(INFO) << "ReadBrightness: " << system_brightness_ << " -> " << level;
    int64 brightness = Clamp(als_brightness_level_ + *brightness_offset_);
    int64 diff = Clamp(brightness + level - system_brightness_) - brightness;
    *brightness_offset_ += diff;
    system_brightness_ = level;
    WritePrefs();
    return false;
  }
  return true;
}

bool BacklightController::WriteBrightness() {
  if (!is_initialized_)
    return false;
  CHECK(brightness_offset_ != NULL) << "Plugged state must be initialized";
  int64 old_brightness = system_brightness_;
  if (state_ == BACKLIGHT_ACTIVE_ON) {
    system_brightness_ = Clamp(als_brightness_level_ + *brightness_offset_);
    if (system_brightness_ == 0)
      system_brightness_ = 1;
  } else if (state_ == BACKLIGHT_DIM) {
    // When in dimmed state, set to dim level only if it results in a reduction
    // of system brightness.
    if (system_brightness_ > kIdleBrightness)
      system_brightness_ = kIdleBrightness;
    else
      LOG(INFO) << "Not dimming because backlight is already dim.";
  } else if (state_ == BACKLIGHT_IDLE_OFF || state_ == BACKLIGHT_ACTIVE_OFF ||
             state_ == BACKLIGHT_SUSPENDED) {
    system_brightness_ = 0;
  }
  als_hysteresis_level_ = als_brightness_level_;
  int64 val = lround(max_ * system_brightness_ / 100.);
  system_brightness_ = Clamp(lround(100. * val / max_));
  LOG(INFO) << "WriteBrightness: " << old_brightness << " -> "
            << system_brightness_;
  if (backlight_->SetBrightness(val))
    WritePrefs();
  return system_brightness_ != old_brightness;
}

void BacklightController::SetBrightnessToZero() {
  if (!is_initialized_)
    return;
  system_brightness_ = 0;
  if (backlight_->SetBrightness(0))
    WritePrefs();
}

void BacklightController::TurnScreenOff() {
  if (state_ == BACKLIGHT_IDLE_OFF && GDK_DISPLAY() != NULL)
    CHECK(DPMSForceLevel(GDK_DISPLAY(), DPMSModeOff));
}


}  // namespace power_manager
