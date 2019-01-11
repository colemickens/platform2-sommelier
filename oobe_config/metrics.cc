// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/metrics.h"

namespace {

// UMA metric names
constexpr char kOobeRestoreResultMetricName[] = "Rollback.OobeRestoreResult";

}  // namespace

namespace oobe_config {

Metrics::Metrics() {
  metrics_library_.Init();
}

void Metrics::RecordRestoreResult(OobeRestoreResult result) {
  metrics_library_.SendEnumToUMA(kOobeRestoreResultMetricName,
                                 static_cast<int>(result),
                                 static_cast<int>(OobeRestoreResult::kCount));
}

}  // namespace oobe_config
