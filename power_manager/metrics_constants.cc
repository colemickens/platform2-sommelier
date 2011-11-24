// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/metrics_constants.h"

namespace power_manager {

const char kMetricBacklightLevelName[] = "Power.BacklightLevel";
const int kMetricBacklightLevelMax = 100;
const time_t kMetricBacklightLevelIntervalMs = 30000;

const int kMetricIdleAfterScreenOffMin = 100;
const int kMetricIdleAfterScreenOffMax = 10 * 60 * 1000;
const int kMetricIdleAfterScreenOffBuckets = 50;

const char kMetricIdleName[] = "Power.IdleTime";
const int kMetricIdleMin = 60 * 1000;
const int kMetricIdleMax = 60 * 60 * 1000;
const int kMetricIdleBuckets = 50;
const char kMetricIdleAfterDimName[] = "Power.IdleTimeAfterDim";
const int kMetricIdleAfterDimMin = 100;
const int kMetricIdleAfterDimMax = 10 * 60 * 1000;
const int kMetricIdleAfterDimBuckets = 50;
const char kMetricIdleAfterScreenOffName[] = "Power.IdleTimeAfterScreenOff";

const char kMetricBatteryDischargeRateName[] =
    "Power.BatteryDischargeRate";  // mW
const int kMetricBatteryDischargeRateMin = 1000;
const int kMetricBatteryDischargeRateMax = 30000;
const int kMetricBatteryDischargeRateBuckets = 50;
const time_t kMetricBatteryDischargeRateInterval = 30;  // seconds

const char kMetricBatteryRemainingChargeName[] =
    "Power.BatteryRemainingCharge";  // %
const int kMetricBatteryRemainingChargeMax = 101;
const time_t kMetricBatteryRemainingChargeInterval = 30;  // seconds
const char kMetricBatteryTimeToEmptyName[] =
    "Power.BatteryTimeToEmpty";  // minutes
const int kMetricBatteryTimeToEmptyMin = 1;
const int kMetricBatteryTimeToEmptyMax = 1000;
const int kMetricBatteryTimeToEmptyBuckets = 50;
const time_t kMetricBatteryTimeToEmptyInterval = 30;  // seconds

const char kMetricBatteryRemainingAtEndOfSessionName[] =
    "Power.BatteryRemainingAtEndOfSession";  // %
const int kMetricBatteryRemainingAtEndOfSessionMax = 101;
const char kMetricBatteryRemainingAtStartOfSessionName[] =
    "Power.BatteryRemainingAtStartOfSession";  // %
const int kMetricBatteryRemainingAtStartOfSessionMax = 101;
const char kMetricNumberOfAlsAdjustmentsPerSessionName[] =
    "Power.NumberOfAlsAdjustmentsPerSession";  // %

const int kMetricNumberOfAlsAdjustmentsPerSessionMin = 0;
const int kMetricNumberOfAlsAdjustmentsPerSessionMax = 10000;
const int kMetricNumberOfAlsAdjustmentsPerSessionBuckets = 50;

const char kMetricPowerButtonDownTimeName[] =
    "Power.PowerButtonDownTime";  // ms
const int kMetricPowerButtonDownTimeMin = 1;
const int kMetricPowerButtonDownTimeMax = 8 * 1000;
const int kMetricPowerButtonDownTimeBuckets = 50;

const char kMetricBrightnessAdjust[] = "Power.BrightnessAdjust";
}  // namespace power_manager
