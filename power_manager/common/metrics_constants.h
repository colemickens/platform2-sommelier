// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_METRICS_CONSTANTS_H_
#define POWER_MANAGER_COMMON_METRICS_CONSTANTS_H_

namespace power_manager {
namespace metrics {

// Suffixes added to certain metric names when on different power sources.
extern const char kAcSuffix[];
extern const char kBatterySuffix[];

// Default max for percent-based metrics. Percents are reported as enums instead
// of regular exponential histograms so they'll get a linear scale.
extern const int kMaxPercent;

// Default number of buckets to use for numeric histogram metrics.
extern const int kDefaultBuckets;

extern const char kSuspendAttemptsBeforeSuccessName[];
extern const char kSuspendAttemptsBeforeCancelName[];
extern const int kSuspendAttemptsMin;
extern const int kSuspendAttemptsMax;
extern const int kSuspendAttemptsBuckets;

extern const char kShutdownReasonName[];
extern const int kShutdownReasonMax;

extern const char kBacklightLevelName[];
extern const char kKeyboardBacklightLevelName[];
extern const int kBacklightLevelIntervalMs;

extern const char kIdleAfterScreenOffName[];
extern const int kIdleAfterScreenOffMin;
extern const int kIdleAfterScreenOffMax;

extern const char kIdleName[];
extern const int kIdleMin;
extern const int kIdleMax;

extern const char kIdleAfterDimName[];
extern const int kIdleAfterDimMin;
extern const int kIdleAfterDimMax;

extern const char kBatteryChargeHealthName[];
extern const int kBatteryChargeHealthMax;

extern const char kBatteryDischargeRateName[];
extern const int kBatteryDischargeRateMin;
extern const int kBatteryDischargeRateMax;
extern const int kBatteryDischargeRateIntervalSec;

extern const char kBatteryDischargeRateWhileSuspendedName[];
extern const int kBatteryDischargeRateWhileSuspendedMin;
extern const int kBatteryDischargeRateWhileSuspendedMax;
extern const int kBatteryDischargeRateWhileSuspendedMinSuspendSec;

extern const char kBatteryRemainingWhenChargeStartsName[];
extern const char kBatteryRemainingAtEndOfSessionName[];
extern const char kBatteryRemainingAtStartOfSessionName[];

extern const char kNumberOfAlsAdjustmentsPerSessionName[];
extern const int kNumberOfAlsAdjustmentsPerSessionMin;
extern const int kNumberOfAlsAdjustmentsPerSessionMax;

extern const char kUserBrightnessAdjustmentsPerSessionName[];
extern const int kUserBrightnessAdjustmentsPerSessionMin;
extern const int kUserBrightnessAdjustmentsPerSessionMax;

extern const char kLengthOfSessionName[];
extern const int kLengthOfSessionMin;
extern const int kLengthOfSessionMax;

extern const char kNumOfSessionsPerChargeName[];
extern const int kNumOfSessionsPerChargeMin;
extern const int kNumOfSessionsPerChargeMax;

extern const char kPowerButtonDownTimeName[];
extern const int kPowerButtonDownTimeMin;
extern const int kPowerButtonDownTimeMax;

extern const char kPowerButtonAcknowledgmentDelayName[];
extern const int kPowerButtonAcknowledgmentDelayMin;
extern const int kPowerButtonAcknowledgmentDelayMax;

extern const char kBatteryInfoSampleName[];

extern const char kPowerSupplyMaxVoltageName[];
extern const int kPowerSupplyMaxVoltageMax;

extern const char kPowerSupplyMaxPowerName[];
extern const int kPowerSupplyMaxPowerMax;

extern const char kPowerSupplyTypeName[];

extern const char kConnectedChargingPortsName[];

extern const char kExternalBrightnessRequestResultName[];
extern const char kExternalBrightnessReadResultName[];
extern const char kExternalBrightnessWriteResultName[];
extern const char kExternalDisplayOpenResultName[];
extern const int kExternalDisplayResultMax;

extern const char kDarkResumeWakeupsPerHourName[];
extern const int kDarkResumeWakeupsPerHourMin;
extern const int kDarkResumeWakeupsPerHourMax;

extern const char kDarkResumeWakeDurationMsName[];
extern const int kDarkResumeWakeDurationMsMin;
extern const int kDarkResumeWakeDurationMsMax;

extern const char kS0ixResidencyRateName[];

// Values for kBatteryInfoSampleName.
enum class BatteryInfoSampleResult {
  READ,
  GOOD,
  BAD,
  MAX,
};

// Values for kPowerSupplyTypeName. Do not renumber.
enum class PowerSupplyType {
  OTHER = 0,
  MAINS = 1,
  USB = 2,
  USB_ACA = 3,
  USB_CDP = 4,
  USB_DCP = 5,
  USB_C = 6,
  USB_PD = 7,
  USB_PD_DRP = 8,
  BRICK_ID = 9,
  // Keep this last and increment it if a new value is inserted.
  MAX = 10,
};

// Values for kConnectedChargingPortsName. Do not renumber.
enum class ConnectedChargingPorts {
  NONE = 0,
  PORT1 = 1,
  PORT2 = 2,
  PORT1_PORT2 = 3,
  TOO_MANY_PORTS = 4,
  // Keep this last and increment it if a new value is inserted.
  MAX = 5,
};

}  // namespace metrics
}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_METRICS_CONSTANTS_H_
