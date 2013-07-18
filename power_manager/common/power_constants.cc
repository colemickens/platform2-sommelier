// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/power_constants.h"

#include "base/stringprintf.h"

namespace power_manager {

const char kLowBatteryShutdownTimePref[] = "low_battery_shutdown_time_s";
const char kLowBatteryShutdownPercentPref[] = "low_battery_shutdown_percent";
const char kCleanShutdownTimeoutMsPref[] = "clean_shutdown_timeout_ms";
const char kPluggedDimMsPref[] = "plugged_dim_ms";
const char kPluggedOffMsPref[] = "plugged_off_ms";
const char kUnpluggedDimMsPref[] = "unplugged_dim_ms";
const char kUnpluggedOffMsPref[] = "unplugged_off_ms";
const char kUnpluggedSuspendMsPref[] = "unplugged_suspend_ms";
const char kDisableIdleSuspendPref[] = "disable_idle_suspend";
const char kUseLidPref[] = "use_lid";
const char kRetrySuspendMsPref[] = "retry_suspend_ms";
const char kRetrySuspendAttemptsPref[] = "retry_suspend_attempts";
const char kPluggedSuspendMsPref[] = "plugged_suspend_ms";
const char kMinVisibleBacklightLevelPref[] = "min_visible_backlight_level";
const char kInstantTransitionsBelowMinLevelPref[] =
    "instant_transitions_below_min_level";
const char kDisableALSPref[] = "disable_als";
const char kKeepBacklightOnForAudioPref[] = "keep_backlight_on_for_audio";
const char kWakeupInputPref[] = "wakeup_input_device_names";
const char kPowerSupplyFullFactorPref[] = "power_supply_full_factor";
const char kInternalBacklightAlsLimitsPref[] = "internal_backlight_als_limits";
const char kInternalBacklightAlsStepsPref[] = "internal_backlight_als_steps";
const char kInternalBacklightNoAlsAcBrightnessPref[] =
    "internal_backlight_no_als_ac_brightness";
const char kInternalBacklightNoAlsBatteryBrightnessPref[] =
    "internal_backlight_no_als_battery_brightness";
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
const char kDisableDarkResumePref[] = "disable_dark_resume";
const char kDarkResumeSuspendDurationsPref[] = "dark_resume_suspend_durations";
const char kDarkResumeDevicesPref[] = "dark_resume_devices";
const char kDarkResumeSourcesPref[] = "dark_resume_sources";
const char kDarkResumeBatteryMarginsPref[] = "dark_resume_battery_margins";
const char kIgnoreExternalPolicyPref[] = "ignore_external_policy";
const char kNumSessionsOnCurrentChargePref[] = "num_sessions_on_current_charge";
const char kAllowDockedModePref[] = "allow_docked_mode";

// Miscellaneous constants.
const char kInternalBacklightPath[] = "/sys/class/backlight";
const char kInternalBacklightPattern[] = "*";
const char kKeyboardBacklightPath[] = "/sys/class/leds";
const char kKeyboardBacklightPattern[] = "*:kbd_backlight";

// Broadcast signals.
const char kPowerStateChanged[] = "PowerStateChanged";

// Unknown shutdown reason.
const char kShutdownReasonUnknown[] = "unknown";
// Explicit user request such as pressing the power button in the Chrome UI.
const char kShutdownReasonUserRequest[] = "user-request";
// Closed the lid at login screen, resulting in shutdown instead of suspend.
const char kShutdownReasonLidClosed[] = "lid-closed";
// Idle at login screen, resulting in shutdown instead of suspend.
const char kShutdownReasonIdle[] = "idle";
// Shutting down due to low battery.
const char kShutdownReasonLowBattery[] = "low-battery";
// Shutting down because suspend attempts failed.
const char kShutdownReasonSuspendFailed[] = "suspend-failed";
// Shutting down from dark resume.
const char kShutdownReasonDarkResume[] = "dark-resume";

const double kEpsilon = 0.001;

const int64 kFastBacklightTransitionMs = 200;
const int64 kSlowBacklightTransitionMs = 2000;

std::string PowerSourceToString(PowerSource source) {
  switch (source) {
    case POWER_AC:
      return "AC";
    case POWER_BATTERY:
      return "battery";
    default:
      return StringPrintf("unknown (%d)", source);
  }
}

std::string LidStateToString(LidState state) {
  switch (state) {
    case LID_OPEN:
      return "open";
    case LID_CLOSED:
      return "closed";
    case LID_NOT_PRESENT:
      return "not present";
    default:
      return StringPrintf("unknown (%d)", state);
  }
}

std::string SessionStateToString(SessionState state) {
  switch (state) {
    case SESSION_STOPPED:
      return "stopped";
    case SESSION_STARTED:
      return "started";
    default:
      return StringPrintf("unknown (%d)", state);
  }
}

std::string UpdaterStateToString(UpdaterState state) {
  switch (state) {
    case UPDATER_IDLE:
      return "idle";
    case UPDATER_UPDATING:
      return "updating";
    case UPDATER_UPDATED:
      return "updated";
    default:
      return StringPrintf("unknown (%d)", state);
  }
}

std::string DisplayModeToString(DisplayMode mode) {
  switch (mode) {
    case DISPLAY_NORMAL:
      return "normal";
    case DISPLAY_PRESENTATION:
      return "presentation";
    default:
      return StringPrintf("unknown (%d)", mode);
  }
}

}  // namespace power_manager
