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

}  // namespace power_manager
