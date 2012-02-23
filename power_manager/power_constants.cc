// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_constants.h"

namespace power_manager {

const char kPluggedBrightnessOffset[] = "plugged_brightness_offset";
const char kUnpluggedBrightnessOffset[] = "unplugged_brightness_offset";
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
const char kMinVisibleBacklightLevel[] = "min_visible_backlight_level";
const char kDisableALS[] = "disable_als";
const char kWakeupInput[] = "wakeup_input_device_names";

// The minimum delta between timers when we want to give a user time to react.
const char kReactMs[] = "react_ms";
// The minimum delta between timers to avoid timer precision issues.
const char kFuzzMs[] = "fuzz_ms";

const char kBacklightPath[] = "/sys/class/backlight";
const char kBacklightPattern[] = "*";
const char kKeyboardBacklightPath[] = "/sys/class/leds";
const char kKeyboardBacklightPattern[] = "*:kbd_backlight";

// Interface names.
const char kRootPowerManagerInterface[] = "org.chromium.RootPowerManager";
const char kRootPowerManagerServiceName[] = "org.chromium.RootPowerManager";

// powerd -> powerm signals.
const char kRestartSignal[] = "RestartSignal";
const char kRequestCleanShutdown[] = "RequestCleanShutdown";
const char kSuspendSignal[] = "SuspendSignal";
const char kShutdownSignal[] = "ShutdownSignal";
const char kExternalBacklightGetMethod[] = "ExternalBacklightGet";
const char kExternalBacklightSetMethod[] = "ExternalBacklightSet";

// powerm -> powerd signals.
const char kLidClosed[] = "LidClosed";
const char kLidOpened[] = "LidOpened";
const char kExternalBacklightUpdate[] = "ExternalBacklightUpdate";

// Broadcast signals.
const char kPowerStateChanged[] = "PowerStateChanged";
const char kPowerSupplyChanged[] = "PowerSupplyChanged";

// Files to signal powerd_suspend whether suspend should be cancelled.
const char kLidOpenFile[] = "lid_opened";
const char kUserActiveFile[] = "user_active";

}  // namespace power_manager
