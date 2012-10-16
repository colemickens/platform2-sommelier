// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/backlight_controller.h"

#include "base/logging.h"

namespace power_manager {

const char* BacklightController::PowerStateToString(PowerState state) {
  switch (state) {
    case BACKLIGHT_ACTIVE:
      return "state(ACTIVE)";
    case BACKLIGHT_DIM:
      return "state(DIM)";
    case BACKLIGHT_ALREADY_DIMMED:
      return "state(ALREADY_DIMMED)";
    case BACKLIGHT_IDLE_OFF:
      return "state(IDLE_OFF)";
    case BACKLIGHT_SUSPENDED:
      return "state(SUSPENDED)";
    case BACKLIGHT_UNINITIALIZED:
      return "state(UNINITIALIZED)";
    default:
      NOTREACHED();
      return "";
  }
}

const char* BacklightController::TransitionStyleToString(
    TransitionStyle style) {
  switch (style) {
    case TRANSITION_INSTANT:
      return "transition(INSTANT)";
    case TRANSITION_FAST:
      return "transition(FAST)";
    case TRANSITION_SLOW:
      return "transition(SLOW)";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace power_manager
