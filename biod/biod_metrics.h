// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_BIOD_METRICS_H_
#define BIOD_BIOD_METRICS_H_

#include <memory>
#include <utility>

#include <base/macros.h>

class MetricsLibraryInterface;

namespace biod {

namespace metrics {

extern const char kFpMatchDurationCapture[];
extern const char kFpMatchDurationMatcher[];
extern const char kFpMatchDurationOverall[];
extern const char kFpNoMatchDurationCapture[];
extern const char kFpNoMatchDurationMatcher[];
extern const char kFpNoMatchDurationOverall[];

}  // namespace metrics

class BiodMetrics {
 public:
  BiodMetrics();
  ~BiodMetrics() = default;

  // Send number of fingers enrolled.
  bool SendEnrolledFingerCount(int finger_count);

  // Is unlocking with FP enabled or not?
  bool SendFpUnlockEnabled(bool enabled);

  // Send matching/capture latency metrics.
  bool SendFpLatencyStats(bool matched,
                          int capture_ms,
                          int match_ms,
                          int overall_ms);

  void SetMetricsLibraryForTesting(
      std::unique_ptr<MetricsLibraryInterface> metrics_lib);

  MetricsLibraryInterface* metrics_library_for_testing() {
    return metrics_lib_.get();
  }

 private:
  std::unique_ptr<MetricsLibraryInterface> metrics_lib_;

  DISALLOW_COPY_AND_ASSIGN(BiodMetrics);
};

}  // namespace biod

#endif  // BIOD_BIOD_METRICS_H_
