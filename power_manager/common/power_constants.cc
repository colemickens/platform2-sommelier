// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/power_constants.h"

namespace power_manager {

const char kPluggedBrightnessOffsetPref[] = "plugged_brightness_offset";
const char kUnpluggedBrightnessOffsetPref[] = "unplugged_brightness_offset";
const char kLowBatteryShutdownTimePref[] = "low_battery_shutdown_time_s";
const char kLowBatteryShutdownPercentPref[] = "low_battery_shutdown_percent";
const char kCleanShutdownTimeoutMsPref[] = "clean_shutdown_timeout_ms";
const char kPluggedDimMsPref[] = "plugged_dim_ms";
const char kPluggedOffMsPref[] = "plugged_off_ms";
const char kUnpluggedDimMsPref[] = "unplugged_dim_ms";
const char kUnpluggedOffMsPref[] = "unplugged_off_ms";
const char kUnpluggedSuspendMsPref[] = "unplugged_suspend_ms";
const char kEnforceLockPref[] = "enforce_lock";
const char kDisableIdleSuspendPref[] = "disable_idle_suspend";
const char kUseLidPref[] = "use_lid";
const char kLockOnIdleSuspendPref[] = "lock_on_idle_suspend";
const char kLockMsPref[] = "lock_ms";
const char kRetrySuspendMsPref[] = "retry_suspend_ms";
const char kRetrySuspendAttemptsPref[] = "retry_suspend_attempts";
const char kPluggedSuspendMsPref[] = "plugged_suspend_ms";
const char kMinVisibleBacklightLevelPref[] = "min_visible_backlight_level";
const char kInstantTransitionsBelowMinLevelPref[] =
    "instant_transitions_below_min_level";
const char kDisableALSPref[] = "disable_als";
const char kKeepBacklightOnForAudioPref[] = "keep_backlight_on_for_audio";
const char kWakeupInputPref[] = "wakeup_input_device_names";
// The minimum delta between timers when we want to give a user time to react.
const char kReactMsPref[] = "react_ms";
// The minimum delta between timers to avoid timer precision issues.
const char kFuzzMsPref[] = "fuzz_ms";
// The maximum duration in seconds the state machine can be disabled for
const char kStateMaxDisabledDurationSecPref[] =
    "state_max_disabled_duration_sec";
const char kSampleWindowMaxPref[] = "sample_window_max";
const char kSampleWindowMinPref[] = "sample_window_min";
const char kTaperTimeMaxPref[] = "taper_time_max_s";
const char kTaperTimeMinPref[] = "taper_time_min_s";
const char kPowerSupplyFullFactorPref[] = "power_supply_full_factor";
const char kKeyboardBacklightAlsLimitsPref[] = "keyboard_backlight_als_limits";
const char kKeyboardBacklightAlsStepsPref[] = "keyboard_backlight_als_steps";
const char kKeyboardBacklightUserLimitsPref[] =
    "keyboard_backlight_user_limits";
const char kKeyboardBacklightUserStepsPref[] = "keyboard_backlight_user_steps";
const char kRequireUsbInputDeviceToSuspendPref[] =
    "require_usb_input_device_to_suspend";
const char kBatteryPollIntervalPref[] = "battery_poll_interval_ms";
const char kBatteryPollShortIntervalPref[] = "battery_poll_short_interval_ms";
const char kTurnOffScreenTimeoutMsPref[] = "turn_off_screen_timeout_ms";

// Miscellaneous constants.
const char kInternalBacklightPath[] = "/sys/class/backlight";
const char kInternalBacklightPattern[] = "*";
const char kKeyboardBacklightPath[] = "/sys/class/leds";
const char kKeyboardBacklightPattern[] = "*:kbd_backlight";
// Time to pin battery at full after going off AC.
const int64 kBatteryPercentPinMs = 3 * 60 * 1000;
// Time taken to taper from pinned percentage to actual percentage.
const int64 kBatteryPercentTaperMs = 7 * 60 * 1000;

// powerd -> powerm messages.
const char kCheckLidStateSignal[] = "CheckLidStateSignal";
const char kRestartSignal[] = "RestartSignal";
const char kRequestCleanShutdown[] = "RequestCleanShutdown";
const char kSuspendSignal[] = "SuspendSignal";
const char kShutdownSignal[] = "ShutdownSignal";
const char kBacklightGetMethod[] = "BacklightGet";
const char kBacklightSetMethod[] = "BacklightSet";

// powerm -> powerd messages.
const char kExternalBacklightUpdateSignal[] = "ExternalBacklightUpdate";

// Broadcast signals.
const char kPowerStateChanged[] = "PowerStateChanged";

// Files to signal powerd_suspend whether suspend should be cancelled.
const char kLidOpenFile[] = "lid_opened";
const char kUserActiveFile[] = "user_active";

// Reasons for shutting down.
const char kShutdownReasonUnknown[] = "unknown";
// Explicit user request such as pressing the power key or the power button in
// the Chrome UI.
const char kShutdownReasonUserRequest[] = "user-request";
// Closed the lid at login screen, resulting in shutdown instead of suspend.
const char kShutdownReasonLidClosed[] = "lid-closed";
// Idle at login screen, resulting in shutdown instead of suspend.
const char kShutdownReasonIdle[] = "idle";
// Shutting down due to low battery.
const char kShutdownReasonLowBattery[] = "low-battery";
// Shutting down because suspend attempts failed.
const char kShutdownReasonSuspendFailed[] = "suspend-failed";

}  // namespace power_manager
