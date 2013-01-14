// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerm/powerman.h"

#include <cmath>

#include "base/logging.h"

namespace power_manager {

const char PowerManDaemon::kMetricRetrySuspendCountName[] =
    "Power.RetrySuspendCount";
const int PowerManDaemon::kMetricRetrySuspendCountMin = 1;
const int PowerManDaemon::kMetricRetrySuspendCountBuckets = 10;

void PowerManDaemon::GenerateMetricsOnResumeEvent() {
  if (retry_suspend_count_ > 0) {
    SendMetric(kMetricRetrySuspendCountName, retry_suspend_count_,
               kMetricRetrySuspendCountMin, retry_suspend_attempts_,
               kMetricRetrySuspendCountBuckets);
  }
}

bool PowerManDaemon::SendMetric(const std::string& name, int sample,
                                int min, int max, int nbuckets) {
  DLOG(INFO) << "Sending metric: " << name << " " << sample << " "
             << min << " " << max << " " << nbuckets;
  return metrics_lib_->SendToUMA(name, sample, min, max, nbuckets);
}

}  // namespace power_manager
