// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_POWER_CONSTANTS_H_
#define POWER_MANAGER_COMMON_POWER_CONSTANTS_H_

#include <string>

#include <base/basictypes.h>

namespace power_manager {

// Preference names.
extern const char kLowBatteryShutdownTimePref[];
extern const char kLowBatteryShutdownPercentPref[];
extern const char kCleanShutdownTimeoutMsPref[];
extern const char kPluggedDimMsPref[];
extern const char kPluggedOffMsPref[];
extern const char kUnpluggedDimMsPref[];
extern const char kUnpluggedOffMsPref[];
extern const char kUnpluggedSuspendMsPref[];
extern const char kDisableIdleSuspendPref[];
extern const char kUseLidPref[];
extern const char kRetrySuspendMsPref[];
extern const char kRetrySuspendAttemptsPref[];
extern const char kPluggedSuspendMsPref[];
extern const char kMinVisibleBacklightLevelPref[];
extern const char kInstantTransitionsBelowMinLevelPref[];
extern const char kDisableALSPref[];
extern const char kAvoidSuspendWhenHeadphoneJackPluggedPref[];
extern const char kWakeupInputPref[];
extern const char kPowerSupplyFullFactorPref[];
extern const char kInternalBacklightAlsLimitsPref[];
extern const char kInternalBacklightAlsStepsPref[];
extern const char kInternalBacklightNoAlsAcBrightnessPref[];
extern const char kInternalBacklightNoAlsBatteryBrightnessPref[];
extern const char kKeyboardBacklightAlsLimitsPref[];
extern const char kKeyboardBacklightAlsStepsPref[];
extern const char kKeyboardBacklightUserLimitsPref[];
extern const char kKeyboardBacklightUserStepsPref[];
extern const char kRequireUsbInputDeviceToSuspendPref[];
extern const char kBatteryPollIntervalPref[];
extern const char kBatteryPollShortIntervalPref[];
extern const char kBatteryStabilizedAfterStartupMsPref[];
extern const char kBatteryStabilizedAfterLinePowerConnectedMsPref[];
extern const char kBatteryStabilizedAfterLinePowerDisconnectedMsPref[];
extern const char kBatteryStabilizedAfterResumeMsPref[];
extern const char kTurnOffScreenTimeoutMsPref[];
extern const char kDisableDarkResumePref[];
extern const char kDarkResumeSuspendDurationsPref[];
extern const char kDarkResumeDevicesPref[];
extern const char kDarkResumeSourcesPref[];
extern const char kDarkResumeBatteryMarginsPref[];
// If true, policies sent by Chrome will be ignored.
extern const char kIgnoreExternalPolicyPref[];
// Allow docked mode if external display present and lid is closed.
extern const char kAllowDockedModePref[];
// Number of user sessions that have been active on the current charge.
// Used to persist the count across reboots for metrics-reporting.
extern const char kNumSessionsOnCurrentChargePref[];
// If true, the device has an ambient light sensor.
extern const char kHasAmbientLightSensorPref[];
// If true, the device has a keyboard backlight.
extern const char kHasKeyboardBacklightPref[];
// If true, the device doesn't have an internal display.
extern const char kExternalDisplayOnlyPref[];
// If true, the device has a legacy ACPI power button that doesn't report button
// releases properly.
extern const char kLegacyPowerButtonPref[];
// If true, disable VT switching before suspending.
extern const char kLockVTBeforeSuspendPref[];
// If true, record suspend and resume timestamps in eventlog using the "mosys"
// command.
extern const char kMosysEventlogPref[];

// Miscellaneous constants.
// Default directories where read/write and read-only powerd preference files
// are stored.
extern const char kReadWritePrefsDir[];
extern const char kReadOnlyPrefsDir[];
// Subdirectory within the read-only prefs dir where board-specific prefs are
// stored.
extern const char kBoardSpecificPrefsSubdir[];
extern const char kInternalBacklightPath[];
extern const char kInternalBacklightPattern[];
extern const char kKeyboardBacklightPath[];
extern const char kKeyboardBacklightPattern[];
extern const char kPowerStatusPath[];

// Broadcast signals.
extern const char kPowerStateChanged[];

// Small value used when comparing floating-point percentages.
extern const double kEpsilon;

// Total time that should be used to gradually animate the backlight level
// to a new brightness, in milliseconds.  Note that some
// BacklightController implementations may not use animated transitions.
extern const int64 kFastBacklightTransitionMs;
extern const int64 kSlowBacklightTransitionMs;

enum PowerSource {
  POWER_AC,
  POWER_BATTERY,
};

enum LidState {
  LID_OPEN,
  LID_CLOSED,
  LID_NOT_PRESENT,
};

enum SessionState {
  SESSION_STOPPED,
  SESSION_STARTED,
};

// Current status of update engine, the system updater.
enum UpdaterState {
  // No update is currently being applied.
  UPDATER_IDLE,
  // An update is being downloaded, verified, or applied.
  UPDATER_UPDATING,
  // An update has been successfully applied and will be used after a reboot.
  UPDATER_UPDATED,
};

enum DisplayMode {
  DISPLAY_NORMAL,
  DISPLAY_PRESENTATION,
};

enum ButtonState {
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_REPEAT,
};

// Reasons for the system being shut down.
// Note: These are reported in a histogram and must not be renumbered.
enum ShutdownReason {
  // Explicit user request (e.g. holding power button).
  SHUTDOWN_REASON_USER_REQUEST     = 0,
  // Request from StateController (e.g. lid was closed or user was inactive).
  SHUTDOWN_REASON_STATE_TRANSITION = 1,
  // Battery level dropped below shutdown threshold.
  SHUTDOWN_REASON_LOW_BATTERY      = 2,
  // Multiple suspend attempts failed.
  SHUTDOWN_REASON_SUSPEND_FAILED   = 3,
  // Battery level was below threshold during dark resume from suspend.
  SHUTDOWN_REASON_DARK_RESUME      = 4,
};

// Returns human-readable descriptions of enum values.
std::string PowerSourceToString(PowerSource source);
std::string LidStateToString(LidState state);
std::string SessionStateToString(SessionState state);
std::string UpdaterStateToString(UpdaterState state);
std::string DisplayModeToString(DisplayMode mode);
std::string ButtonStateToString(ButtonState state);
std::string ShutdownReasonToString(ShutdownReason reason);

}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_POWER_CONSTANTS_H_
