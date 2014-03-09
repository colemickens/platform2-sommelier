// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_METRICS_CONSTANTS_H_
#define POWER_MANAGER_COMMON_METRICS_CONSTANTS_H_
#pragma once

#include <time.h>

namespace power_manager {

// Suffixes added to certain metric names when on different power sources.
extern const char kMetricACSuffix[];
extern const char kMetricBatterySuffix[];

// Default max for percent-based metrics. Percents are reported as enums instead
// of regular exponential histograms so they'll get a linear scale.
extern const int kMetricMaxPercent;

// Default number of buckets to use for numeric histogram metrics.
extern const int kMetricDefaultBuckets;

extern const char kMetricSuspendAttemptsBeforeSuccessName[];
extern const char kMetricSuspendAttemptsBeforeCancelName[];
extern const int kMetricSuspendAttemptsMin;
extern const int kMetricSuspendAttemptsMax;
extern const int kMetricSuspendAttemptsBuckets;

extern const char kMetricShutdownReasonName[];
extern const int kMetricShutdownReasonMax;

extern const char kMetricBacklightLevelName[];
extern const char kMetricKeyboardBacklightLevelName[];
extern const time_t kMetricBacklightLevelIntervalMs;

extern const char kMetricIdleAfterScreenOffName[];
extern const int kMetricIdleAfterScreenOffMin;
extern const int kMetricIdleAfterScreenOffMax;

extern const char kMetricIdleName[];
extern const int kMetricIdleMin;
extern const int kMetricIdleMax;

extern const char kMetricIdleAfterDimName[];
extern const int kMetricIdleAfterDimMin;
extern const int kMetricIdleAfterDimMax;

extern const char kMetricBatteryChargeHealthName[];
extern const int kMetricBatteryChargeHealthMax;

extern const char kMetricBatteryDischargeRateName[];
extern const int kMetricBatteryDischargeRateMin;
extern const int kMetricBatteryDischargeRateMax;
extern const time_t kMetricBatteryDischargeRateInterval;

extern const char kMetricBatteryDischargeRateWhileSuspendedName[];
extern const int kMetricBatteryDischargeRateWhileSuspendedMin;
extern const int kMetricBatteryDischargeRateWhileSuspendedMax;
extern const int kMetricBatteryDischargeRateWhileSuspendedMinSuspendSec;

extern const char kMetricBatteryRemainingWhenChargeStartsName[];
extern const char kMetricBatteryRemainingAtEndOfSessionName[];
extern const char kMetricBatteryRemainingAtStartOfSessionName[];

extern const char kMetricNumberOfAlsAdjustmentsPerSessionName[];
extern const int kMetricNumberOfAlsAdjustmentsPerSessionMin;
extern const int kMetricNumberOfAlsAdjustmentsPerSessionMax;

extern const char kMetricUserBrightnessAdjustmentsPerSessionName[];
extern const int kMetricUserBrightnessAdjustmentsPerSessionMin;
extern const int kMetricUserBrightnessAdjustmentsPerSessionMax;

extern const char kMetricLengthOfSessionName[];
extern const int kMetricLengthOfSessionMin;
extern const int kMetricLengthOfSessionMax;

extern const char kMetricNumOfSessionsPerChargeName[];
extern const int kMetricNumOfSessionsPerChargeMin;
extern const int kMetricNumOfSessionsPerChargeMax;

extern const char kMetricPowerButtonDownTimeName[];
extern const int kMetricPowerButtonDownTimeMin;
extern const int kMetricPowerButtonDownTimeMax;

extern const char kMetricPowerButtonAcknowledgmentDelayName[];
extern const int kMetricPowerButtonAcknowledgmentDelayMin;
extern const int kMetricPowerButtonAcknowledgmentDelayMax;

extern const char kMetricBatteryInfoSampleName[];

extern const char kMetricExternalBrightnessRequestResultName[];
extern const char kMetricExternalBrightnessReadResultName[];
extern const char kMetricExternalBrightnessWriteResultName[];
extern const char kMetricExternalDisplayOpenResultName[];
extern const int kMetricExternalDisplayResultMax;

// Enum for kMetricBatteryInfoSample.
enum BatteryInfoSampleResult {
  BATTERY_INFO_READ,
  BATTERY_INFO_GOOD,
  BATTERY_INFO_BAD,
  BATTERY_INFO_MAX,
};

}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_METRICS_CONSTANTS_H_
