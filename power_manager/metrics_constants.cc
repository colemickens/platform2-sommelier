// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/metrics_constants.h"

namespace power_manager {

const char kMetricBacklightLevelName[] = "Power.BacklightLevel";  // %

const int kMetricIdleAfterScreenOffMin = 100;
const int kMetricIdleAfterScreenOffMax = 10 * 60 * 1000;
const int kMetricIdleAfterScreenOffBuckets = 50;

}  // namespace power_manager
