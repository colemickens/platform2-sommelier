// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/metrics_constants.h"

namespace power_manager {
namespace metrics {

const char kAcSuffix[] = "OnAC";
const char kBatterySuffix[] = "OnBattery";

const int kMaxPercent = 101;
const int kDefaultBuckets = 50;

const char kSuspendAttemptsBeforeSuccessName[] =
    "Power.SuspendAttemptsBeforeSuccess";
const char kSuspendAttemptsBeforeCancelName[] =
    "Power.SuspendAttemptsBeforeCancel";
const int kSuspendAttemptsMin = 1;
const int kSuspendAttemptsMax = 20;
const int kSuspendAttemptsBuckets =
    kSuspendAttemptsMax - kSuspendAttemptsMin + 1;

const char kShutdownReasonName[] = "Power.ShutdownReason";
const int kShutdownReasonMax = 10;

const char kBacklightLevelName[] = "Power.BacklightLevel";
const char kKeyboardBacklightLevelName[] = "Power.KeyboardBacklightLevel";
const int kBacklightLevelIntervalMs = 30000;

const char kIdleAfterScreenOffName[] = "Power.IdleTimeAfterScreenOff";
const int kIdleAfterScreenOffMin = 100;
const int kIdleAfterScreenOffMax = 10 * 60 * 1000;

const char kIdleName[] = "Power.IdleTime";
const int kIdleMin = 60 * 1000;
const int kIdleMax = 60 * 60 * 1000;

const char kIdleAfterDimName[] = "Power.IdleTimeAfterDim";
const int kIdleAfterDimMin = 100;
const int kIdleAfterDimMax = 10 * 60 * 1000;

const char kBatteryChargeHealthName[] = "Power.BatteryChargeHealth";  // %
// >100% to account for new batteries which often charge above full
const int kBatteryChargeHealthMax = 111;

const char kBatteryDischargeRateName[] = "Power.BatteryDischargeRate";  // mW
const int kBatteryDischargeRateMin = 1000;
const int kBatteryDischargeRateMax = 30000;
const int kBatteryDischargeRateIntervalSec = 30;

const char kBatteryDischargeRateWhileSuspendedName[] =
    "Power.BatteryDischargeRateWhileSuspended";  // mW
const int kBatteryDischargeRateWhileSuspendedMin = 1;
const int kBatteryDischargeRateWhileSuspendedMax = 30000;
const int kBatteryDischargeRateWhileSuspendedMinSuspendSec = 60;

const char kBatteryRemainingWhenChargeStartsName[] =
    "Power.BatteryRemainingWhenChargeStarts";  // %
const char kBatteryRemainingAtEndOfSessionName[] =
    "Power.BatteryRemainingAtEndOfSession";  // %
const char kBatteryRemainingAtStartOfSessionName[] =
    "Power.BatteryRemainingAtStartOfSession";  // %

const char kNumberOfAlsAdjustmentsPerSessionName[] =
    "Power.NumberOfAlsAdjustmentsPerSession";
const int kNumberOfAlsAdjustmentsPerSessionMin = 1;
const int kNumberOfAlsAdjustmentsPerSessionMax = 10000;

const char kUserBrightnessAdjustmentsPerSessionName[] =
    "Power.UserBrightnessAdjustmentsPerSession";
const int kUserBrightnessAdjustmentsPerSessionMin = 1;
const int kUserBrightnessAdjustmentsPerSessionMax = 10000;

const char kLengthOfSessionName[] = "Power.LengthOfSession";
const int kLengthOfSessionMin = 1;
const int kLengthOfSessionMax = 60 * 60 * 12;

const char kNumOfSessionsPerChargeName[] = "Power.NumberOfSessionsPerCharge";
const int kNumOfSessionsPerChargeMin = 1;
const int kNumOfSessionsPerChargeMax = 10000;

const char kPowerButtonDownTimeName[] = "Power.PowerButtonDownTime";  // ms
const int kPowerButtonDownTimeMin = 1;
const int kPowerButtonDownTimeMax = 8 * 1000;

const char kPowerButtonAcknowledgmentDelayName[] =
    "Power.PowerButtonAcknowledgmentDelay";  // ms
const int kPowerButtonAcknowledgmentDelayMin = 1;
const int kPowerButtonAcknowledgmentDelayMax = 8 * 1000;

const char kBatteryInfoSampleName[] = "Power.BatteryInfoSample";

const char kPowerSupplyMaxVoltageName[] = "Power.PowerSupplyMaxVoltage";
const int kPowerSupplyMaxVoltageMax = 21;

const char kPowerSupplyMaxPowerName[] = "Power.PowerSupplyMaxPower";
const int kPowerSupplyMaxPowerMax = 101;

const char kPowerSupplyTypeName[] = "Power.PowerSupplyType";

const char kConnectedChargingPortsName[] = "Power.ConnectedChargingPorts";

const char kExternalBrightnessRequestResultName[] =
    "Power.ExternalBrightnessRequestResult";
const char kExternalBrightnessReadResultName[] =
    "Power.ExternalBrightnessReadResult";
const char kExternalBrightnessWriteResultName[] =
    "Power.ExternalBrightnessWriteResult";
const char kExternalDisplayOpenResultName[] = "Power.ExternalDisplayOpenResult";
const int kExternalDisplayResultMax = 10;

const char kDarkResumeWakeupsPerHourName[] = "Power.DarkResumeWakeupsPerHour";
const int kDarkResumeWakeupsPerHourMin = 0;
const int kDarkResumeWakeupsPerHourMax = 60 * 60;

const char kDarkResumeWakeDurationMsName[] = "Power.DarkResumeWakeDurationMs";
const int kDarkResumeWakeDurationMsMin = 0;
const int kDarkResumeWakeDurationMsMax = 10 * 60 * 1000;

const char kS0ixResidencyRateName[] = "Power.S0ixResidencyRate";  // %

}  // namespace metrics
}  // namespace power_manager
