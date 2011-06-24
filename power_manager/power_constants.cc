// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_constants.h"

namespace power_manager {

const char kPluggedBrightnessOffset[] = "plugged_brightness_offset";
const char kUnpluggedBrightnessOffset[] = "unplugged_brightness_offset";
const char kAlsBrightnessLevel[] = "als_brightness_level";
const char kLowBatterySuspendPercent[] = "low_battery_suspend_percent";
const char kCleanShutdownTimeoutMs[] = "clean_shutdown_timeout_ms";
const char kPluggedDimMs[] = "plugged_dim_ms";
const char kPluggedOffMs[] = "plugged_off_ms";
const char kUnpluggedDimMs[] = "unplugged_dim_ms";
const char kUnpluggedOffMs[] = "unplugged_off_ms";
const char kUnpluggedSuspendMs[] = "unplugged_suspend_ms";
const char kEnforceLock[] = "enforce_lock";
const char kDisableIdleSuspend[] = "disable_idle_suspend";
const char kUseLid[] = "use_lid";
const char kLockOnIdleSuspend[] = "lock_on_idle_suspend";
const char kLockMs[] = "lock_ms";
const char kRetrySuspendMs[] = "retry_suspend_ms";
const char kRetrySuspendAttempts[] = "retry_suspend_attempts";
const char kPluggedSuspendMs[] = "plugged_suspend_ms";
const char kUseXScreenSaver[] = "use_xscreensaver";
const char kMinBacklightPercent[] = "min_backlight_percent";

// The minimum delta between timers when we want to give a user time to react.
const char kReactMs[] = "react_ms";
// The minimum delta between timers to avoid timer precision issues.
const char kFuzzMs[] = "fuzz_ms";

const char kBacklightPath[] = "/sys/class/backlight";
const char kBacklightPattern[] = "*";

}  // namespace power_manager
