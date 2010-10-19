// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
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
      state_(BACKLIGHT_ACTIVE),
      plugged_state_(kPowerUnknown),
      system_brightness_(0),
      min_(0),
      max_(-1) {}

bool BacklightController::Init() {
  int64 level;
  if (backlight_->GetBrightness(&level, &max_)) {
    ReadPrefs();
    return true;
  }
  return false;
}

void BacklightController::GetBrightness(int64* level) {
  int64 raw_level;
  CHECK(backlight_->GetBrightness(&raw_level, &max_));
  *level = lround(100. * raw_level / max_);
}

void BacklightController::IncreaseBrightness() {
  if (ReadBrightness()) {
    // Give the user between 8 and 16 distinct brightness levels
    int64 offset = 1 + (max_ >> 4);
    int64 new_val = offset + lround(max_ * system_brightness_ / 100.);
    int64 new_brightness = Clamp(lround(100. * new_val / max_));
    if (new_brightness != system_brightness_) {
      // Allow large swing in brightness_offset for absolute brightness
      // outside of clamped brightness region.
      int64 absolute_brightness = als_brightness_level_ + *brightness_offset_;
      *brightness_offset_ += new_brightness - absolute_brightness;
      WriteBrightness();
    }
  }
}

void BacklightController::DecreaseBrightness() {
  if (ReadBrightness()) {
    // Give the user between 8 and 16 distinct brightness levels
    int64 offset = 1 + (max_ >> 4);
    int64 new_val = lround(max_ * system_brightness_ / 100.) - offset;
    int64 new_brightness = Clamp(lround(100. * new_val / max_));
    if (new_brightness != system_brightness_) {
      // Allow large swing in brightness_offset for absolute brightness
      // outside of clamped brightness region.
      int64 absolute_brightness = als_brightness_level_ + *brightness_offset_;
      *brightness_offset_ += new_brightness - absolute_brightness;
      WriteBrightness();
    }
  }
}

void BacklightController::SetDimState(DimState state) {
  if (state != state_) {
    ReadBrightness();
    state_ = state;
    WriteBrightness();
  }
}

void BacklightController::SetPowerState(PowerState state) {
  if (!DPMSCapable(GDK_DISPLAY())) {
    LOG(WARNING) << "X Server is not DPMS capable";
  } else {
    CHECK(DPMSEnable(GDK_DISPLAY()));
    if (state == BACKLIGHT_OFF)
      CHECK(DPMSForceLevel(GDK_DISPLAY(), DPMSModeOff));
    else if (state == BACKLIGHT_ON)
      CHECK(DPMSForceLevel(GDK_DISPLAY(), DPMSModeOn));
    else
      NOTREACHED();
  }

  if (light_sensor_)
    light_sensor_->EnableOrDisableSensor(state, state_);
}

void BacklightController::OnPlugEvent(bool is_plugged) {
  if (brightness_offset_ && is_plugged == plugged_state_)
    return;
  if (is_plugged) {
    brightness_offset_ = &plugged_brightness_offset_;
    plugged_state_ = kPowerConnected;
  } else {
    brightness_offset_ = &unplugged_brightness_offset_;
    plugged_state_ = kPowerDisconnected;
  }
  WriteBrightness();
}

bool BacklightController::ReadBrightness() {
  CHECK(max_ >= 0) << "Init() must be called";
  CHECK(brightness_offset_) << "Plugged state must be initialized";
  int64 level;
  GetBrightness(&level);
  if (level != system_brightness_) {
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

int64 BacklightController::WriteBrightness() {
  CHECK(brightness_offset_) << "Plugged state must be initialized";
  int64 old_brightness = system_brightness_;
  if (state_ == BACKLIGHT_ACTIVE)
    system_brightness_ = Clamp(als_brightness_level_ + *brightness_offset_);
  else
    system_brightness_ = 0;
  als_hysteresis_level_ = als_brightness_level_;
  int64 val = lround(max_ * system_brightness_ / 100.);
  system_brightness_ = Clamp(lround(100. * val / max_));
  LOG(INFO) << "WriteBrightness: " << old_brightness << " -> "
            << system_brightness_;
  CHECK(backlight_->SetBrightness(val));
  WritePrefs();
  return system_brightness_;
}

void BacklightController::SetAlsBrightnessLevel(int64 level) {
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
  if (plugged_state_ == kPowerConnected) {
    prefs_->SetInt64(kPluggedBrightnessOffset,
                     plugged_brightness_offset_);
  } else if (plugged_state_ == kPowerDisconnected) {
    prefs_->SetInt64(kUnpluggedBrightnessOffset,
                     unplugged_brightness_offset_);
  }
}

}  // namespace power_manager
