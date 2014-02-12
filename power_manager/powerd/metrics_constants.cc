// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/metrics_constants.h"

namespace power_manager {

const char kMetricACSuffix[] = "OnAC";
const char kMetricBatterySuffix[] = "OnBattery";

const int kMetricMaxPercent = 101;
const int kMetricDefaultBuckets = 50;

const char kMetricSuspendAttemptsBeforeSuccessName[] =
    "Power.SuspendAttemptsBeforeSuccess";
const char kMetricSuspendAttemptsBeforeCancelName[] =
    "Power.SuspendAttemptsBeforeCancel";
const int kMetricSuspendAttemptsMin = 1;
const int kMetricSuspendAttemptsMax = 20;
const int kMetricSuspendAttemptsBuckets =
    kMetricSuspendAttemptsMax - kMetricSuspendAttemptsMin + 1;

const char kMetricShutdownReasonName[] = "Power.ShutdownReason";
const int kMetricShutdownReasonMax = 10;

const char kMetricBacklightLevelName[] = "Power.BacklightLevel";
const char kMetricKeyboardBacklightLevelName[] = "Power.KeyboardBacklightLevel";
const time_t kMetricBacklightLevelIntervalMs = 30000;

const char kMetricIdleAfterScreenOffName[] = "Power.IdleTimeAfterScreenOff";
const int kMetricIdleAfterScreenOffMin = 100;
const int kMetricIdleAfterScreenOffMax = 10 * 60 * 1000;

const char kMetricIdleName[] = "Power.IdleTime";
const int kMetricIdleMin = 60 * 1000;
const int kMetricIdleMax = 60 * 60 * 1000;

const char kMetricIdleAfterDimName[] = "Power.IdleTimeAfterDim";
const int kMetricIdleAfterDimMin = 100;
const int kMetricIdleAfterDimMax = 10 * 60 * 1000;

const char kMetricBatteryChargeHealthName[] =
    "Power.BatteryChargeHealth";  // %
// >100% to account for new batteries which often charge above full
const int kMetricBatteryChargeHealthMax = 111;

const char kMetricBatteryDischargeRateName[] =
    "Power.BatteryDischargeRate";  // mW
const int kMetricBatteryDischargeRateMin = 1000;
const int kMetricBatteryDischargeRateMax = 30000;
const time_t kMetricBatteryDischargeRateInterval = 30;  // seconds

const char kMetricBatteryDischargeRateWhileSuspendedName[] =
    "Power.BatteryDischargeRateWhileSuspended";  // mW
const int kMetricBatteryDischargeRateWhileSuspendedMin = 1;
const int kMetricBatteryDischargeRateWhileSuspendedMax = 30000;
const int kMetricBatteryDischargeRateWhileSuspendedMinSuspendSec = 60;

const char kMetricBatteryRemainingWhenChargeStartsName[] =
    "Power.BatteryRemainingWhenChargeStarts";  // %
const char kMetricBatteryRemainingAtEndOfSessionName[] =
    "Power.BatteryRemainingAtEndOfSession";  // %
const char kMetricBatteryRemainingAtStartOfSessionName[] =
    "Power.BatteryRemainingAtStartOfSession";  // %

const char kMetricNumberOfAlsAdjustmentsPerSessionName[] =
    "Power.NumberOfAlsAdjustmentsPerSession";
const int kMetricNumberOfAlsAdjustmentsPerSessionMin = 1;
const int kMetricNumberOfAlsAdjustmentsPerSessionMax = 10000;

const char kMetricUserBrightnessAdjustmentsPerSessionName[] =
    "Power.UserBrightnessAdjustmentsPerSession";
const int kMetricUserBrightnessAdjustmentsPerSessionMin = 1;
const int kMetricUserBrightnessAdjustmentsPerSessionMax = 10000;

const char kMetricLengthOfSessionName[] =
    "Power.LengthOfSession";
const int kMetricLengthOfSessionMin = 1;
const int kMetricLengthOfSessionMax = 60 * 60 * 12;

const char kMetricNumOfSessionsPerChargeName[] =
    "Power.NumberOfSessionsPerCharge";
const int kMetricNumOfSessionsPerChargeMin = 1;
const int kMetricNumOfSessionsPerChargeMax = 10000;

const char kMetricPowerButtonDownTimeName[] =
    "Power.PowerButtonDownTime";  // ms
const int kMetricPowerButtonDownTimeMin = 1;
const int kMetricPowerButtonDownTimeMax = 8 * 1000;

const char kMetricPowerButtonAcknowledgmentDelayName[] =
    "Power.PowerButtonAcknowledgmentDelay";  // ms
const int kMetricPowerButtonAcknowledgmentDelayMin = 1;
const int kMetricPowerButtonAcknowledgmentDelayMax = 8 * 1000;

const char kMetricBatteryInfoSampleName[] = "Power.BatteryInfoSample";

}  // namespace power_manager
