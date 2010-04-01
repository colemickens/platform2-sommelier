// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/idle_dimmer.h"
#include <algorithm>
#include "base/logging.h"

using std::min;

namespace power_manager {

IdleDimmer::IdleDimmer(int64 idle_brightness, BacklightInterface* backlight)
    : idle_dim_(false),
      idle_brightness_(idle_brightness),
      backlight_(backlight) {
}

void IdleDimmer::OnIdleEvent(bool is_idle, int64) {
  int64 level, max_level, new_level;
  if (!backlight_->GetBrightness(&level, &max_level))
    return;
  if (is_idle) {
    if (idle_brightness_ >= level) {
      LOG(INFO) << "Monitor is already dim. Nothing to do.";
      return;
    }
    if (idle_dim_) {
      LOG(WARNING) << "Ignoring duplicate idle event.";
      return;
    }
    new_level = idle_brightness_;
    active_brightness_ = level;
    idle_dim_ = true;
    LOG(INFO) << "Dim from " << level << " to " << idle_brightness_
              << " (out of " << max_level << ")";
  } else {  // !is_idle
    if (!idle_dim_) {
      LOG(INFO) << "Monitor is already bright. Nothing to do.";
      return;
    }
    int64 diff = level - idle_brightness_;
    new_level = min(active_brightness_ + diff, max_level);
    idle_dim_ = false;
    LOG(INFO) << "Brighten from " << level << " to " << new_level
              << " (out of " << max_level << ")";
  }
  backlight_->SetBrightness(new_level);
}
}  // namespace power_manager
