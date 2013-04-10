// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_POWER_CONSTANTS_H_
#define POWER_MANAGER_COMMON_POWER_CONSTANTS_H_

#include <string>

#include "base/basictypes.h"

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
extern const char kKeepBacklightOnForAudioPref[];
extern const char kWakeupInputPref[];
extern const char kStateMaxDisabledDurationSecPref[];
extern const char kSampleWindowMaxPref[];
extern const char kSampleWindowMinPref[];
extern const char kTaperTimeMaxPref[];
extern const char kTaperTimeMinPref[];
extern const char kPowerSupplyFullFactorPref[];
extern const char kInternalBacklightAlsLimitsPref[];
extern const char kInternalBacklightAlsStepsPref[];
extern const char kKeyboardBacklightAlsLimitsPref[];
extern const char kKeyboardBacklightAlsStepsPref[];
extern const char kKeyboardBacklightUserLimitsPref[];
extern const char kKeyboardBacklightUserStepsPref[];
extern const char kRequireUsbInputDeviceToSuspendPref[];
extern const char kBatteryPollIntervalPref[];
extern const char kBatteryPollShortIntervalPref[];
extern const char kTurnOffScreenTimeoutMsPref[];
extern const char kDisableDarkResumePref[];
extern const char kDarkResumeSuspendDurationsPref[];
extern const char kDarkResumeDevicesPref[];
extern const char kDarkResumeSourcesPref[];
extern const char kDarkResumeBatteryMarginsPref[];
// Non-zero if the system should suspend instead of shutting down due to
// user inactivity at the login screen.
extern const char kSuspendAtLoginScreenPref[];

// Miscellaneous constants.
extern const char kInternalBacklightPath[];
extern const char kInternalBacklightPattern[];
extern const char kKeyboardBacklightPath[];
extern const char kKeyboardBacklightPattern[];
extern const int64 kBatteryPercentPinMs;
extern const int64 kBatteryPercentTaperMs;

// Broadcast signals.
extern const char kPowerStateChanged[];

// Reasons for shutting down
extern const char kShutdownReasonUnknown[];
extern const char kShutdownReasonUserRequest[];
extern const char kShutdownReasonLidClosed[];
extern const char kShutdownReasonIdle[];
extern const char kShutdownReasonLowBattery[];
extern const char kShutdownReasonSuspendFailed[];

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

// Returns human-readable descriptions of enum values.
std::string PowerSourceToString(PowerSource source);
std::string LidStateToString(LidState state);
std::string SessionStateToString(SessionState state);
std::string UpdaterStateToString(UpdaterState state);
std::string DisplayModeToString(DisplayMode mode);

}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_POWER_CONSTANTS_H_
