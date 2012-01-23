// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_CONSTANTS_H_
#define POWER_MANAGER_POWER_CONSTANTS_H_

namespace power_manager {

extern const char kPluggedBrightnessOffset[];
extern const char kUnpluggedBrightnessOffset[];
extern const char kAlsBrightnessLevel[];
extern const char kLowBatterySuspendPercent[];
extern const char kCleanShutdownTimeoutMs[];
extern const char kPluggedDimMs[];
extern const char kPluggedOffMs[];
extern const char kUnpluggedDimMs[];
extern const char kUnpluggedOffMs[];
extern const char kUnpluggedSuspendMs[];
extern const char kEnforceLock[];
extern const char kDisableIdleSuspend[];
extern const char kUseLid[];
extern const char kLockOnIdleSuspend[];
extern const char kLockMs[];
extern const char kRetrySuspendMs[];
extern const char kRetrySuspendAttempts[];
extern const char kUseXScreenSaver[];
extern const char kPluggedSuspendMs[];
extern const char kMinBacklightLevel[];
extern const char kReactMs[];
extern const char kFuzzMs[];
extern const char kBacklightPath[];
extern const char kBacklightPattern[];
extern const char kKeyboardBacklightPath[];
extern const char kKeyboardBacklightPattern[];
extern const char kDisableALS[];

// Interface names.
extern const char kRootPowerManagerInterface[];
extern const char kRootPowerManagerServiceName[];

// powerd -> powerm constants.
extern const char kRestartSignal[];
extern const char kRequestCleanShutdown[];
extern const char kSuspendSignal[];
extern const char kShutdownSignal[];
extern const char kExternalBacklightGetMethod[];
extern const char kExternalBacklightSetMethod[];

// powerm -> powerd constants.
extern const char kLidClosed[];
extern const char kLidOpened[];
extern const char kPowerButtonDown[];
extern const char kPowerButtonUp[];
extern const char kExternalBacklightUpdate[];

// Broadcast signals.
extern const char kPowerStateChanged[];
extern const char kPowerSupplyChanged[];

// Files to signal powerd_suspend whether suspend should be cancelled.
extern const char kLidOpenFile[];
extern const char kUserActiveFile[];

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_CONSTANTS_H_
