// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_METRICS_CONSTANTS_H_
#define POWER_MANAGER_METRICS_CONSTANTS_H_

#include <time.h>

namespace power_manager {

extern const char kMetricBacklightLevelName[];
extern const int kMetricBacklightLevelMax;
extern const time_t kMetricBacklightLevelIntervalMs;
extern const int kMetricIdleAfterScreenOffMin;
extern const int kMetricIdleAfterScreenOffMax;
extern const int kMetricIdleAfterScreenOffBuckets;
extern const char kMetricIdleName[];
extern const int kMetricIdleMin;
extern const int kMetricIdleMax;
extern const int kMetricIdleBuckets;
extern const char kMetricIdleAfterDimName[];
extern const int kMetricIdleAfterDimMin;
extern const int kMetricIdleAfterDimMax;
extern const int kMetricIdleAfterDimBuckets;
extern const char kMetricIdleAfterScreenOffName[];
extern const char kMetricBatteryDischargeRateName[];
extern const int kMetricBatteryDischargeRateMin;
extern const int kMetricBatteryDischargeRateMax;
extern const int kMetricBatteryDischargeRateBuckets;
extern const time_t kMetricBatteryDischargeRateInterval;

}  // namespace power_manager

#endif  // POWER_MANAGER_METRICS_CONSTANTS_H_
