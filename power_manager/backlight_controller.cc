// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/backlight_controller.h"

#include <gdk/gdkx.h>
#include <math.h>
#include <X11/extensions/dpms.h>

#include "base/logging.h"

namespace power_manager {

BacklightController::BacklightController(BacklightInterface* backlight,
                                         PowerPrefsInterface *prefs)
    : backlight_(backlight),
      prefs_(prefs),
      als_brightness_level_(0),
      plugged_brightness_offset_(-1),
      unplugged_brightness_offset_(-1),
      brightness_offset_(NULL),
      state_(BACKLIGHT_ACTIVE),
      plugged_state_(POWER_UNKNOWN),
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

void BacklightController::ChangeBrightness(int64 diff) {
  ReadBrightness();
  int64 brightness = clamp(als_brightness_level_ + *brightness_offset_);
  *brightness_offset_ += clamp(brightness + diff) - brightness;
  WriteBrightness();
}

void BacklightController::SetDimState(DimState state) {
  if (state != state_) {
    ReadBrightness();
    state_ = state;
    WriteBrightness();
  }
}

void BacklightController::SetPowerState(PowerState state) {
  if (state == BACKLIGHT_OFF)
    DPMSForceLevel(GDK_DISPLAY(), DPMSModeOff);
  else if (state == BACKLIGHT_ON)
    DPMSForceLevel(GDK_DISPLAY(), DPMSModeOn);
  else
    NOTREACHED();
}

void BacklightController::OnPlugEvent(bool is_plugged) {
  if (brightness_offset_ && is_plugged == plugged_state_)
    return;
  if (brightness_offset_)
    ReadBrightness();
  if (is_plugged) {
    brightness_offset_ = &plugged_brightness_offset_;
    plugged_state_ = POWER_CONNECTED;
  } else {
    brightness_offset_ = &unplugged_brightness_offset_;
    plugged_state_ = POWER_DISCONNECTED;
  }
  WriteBrightness();
}

int64 BacklightController::ReadBrightness() {
  CHECK(max_ >= 0) << "Init() must be called";
  CHECK(brightness_offset_) << "Plugged state must be initialized";
  int64 level;
  GetBrightness(&level);
  if (level != system_brightness_) {
    // Another program adjusted the brightness. Sync up.
    int64 brightness = clamp(als_brightness_level_ + *brightness_offset_);
    int64 diff = clamp(brightness + level - system_brightness_) - brightness;
    *brightness_offset_ += diff;
    system_brightness_ = level;
  }
  return level;
}

int64 BacklightController::WriteBrightness() {
  CHECK(brightness_offset_) << "Plugged state must be initialized";
  int64 old_brightness = system_brightness_;
  if (state_ == BACKLIGHT_ACTIVE)
    system_brightness_ = clamp(als_brightness_level_ + *brightness_offset_);
  else
    system_brightness_ = 0;
  int64 val = lround(max_ * system_brightness_ / 100.);
  LOG(INFO) << "Brightness: " << old_brightness << " -> "
            << system_brightness_;
  CHECK(backlight_->SetBrightness(val));
  WritePrefs();
  return system_brightness_;
}

void BacklightController::ReadPrefs() {
  CHECK(prefs_->ReadSetting("plugged_brightness_offset",
                            &plugged_brightness_offset_));
  CHECK(prefs_->ReadSetting("unplugged_brightness_offset",
                            &unplugged_brightness_offset_));
  CHECK(plugged_brightness_offset_ >= -100);
  CHECK(plugged_brightness_offset_ <= 100);
  CHECK(unplugged_brightness_offset_ >= -100);
  CHECK(unplugged_brightness_offset_ <= 100);
}

void BacklightController::WritePrefs() {
  if (plugged_state_ == POWER_CONNECTED) {
    CHECK(prefs_->WriteSetting("plugged_brightness_offset",
                               plugged_brightness_offset_));
  } else if (plugged_state_ == POWER_DISCONNECTED) {
    CHECK(prefs_->WriteSetting("unplugged_brightness_offset",
                               unplugged_brightness_offset_));
  }
}

}  // namespace power_manager
