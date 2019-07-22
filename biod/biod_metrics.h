// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_BIOD_METRICS_H_
#define BIOD_BIOD_METRICS_H_

#include <memory>
#include <utility>

#include <base/macros.h>
#include <metrics/metrics_library.h>

#include "biod/update_reason.h"

namespace biod {

namespace metrics {

extern const char kFpMatchDurationCapture[];
extern const char kFpMatchDurationMatcher[];
extern const char kFpMatchDurationOverall[];
extern const char kFpNoMatchDurationCapture[];
extern const char kFpNoMatchDurationMatcher[];
extern const char kFpNoMatchDurationOverall[];
extern const char kUpdaterStatus[];
extern const char kUpdaterReason[];
extern const char kUpdaterDurationNoUpdate[];
extern const char kUpdaterDurationUpdate[];

}  // namespace metrics

class BiodMetrics {
 public:
  BiodMetrics();
  ~BiodMetrics() = default;

  // This is the tools/bio_fw_updater overall status,
  // which encapsulates an UpdateStatus.
  enum class FwUpdaterStatus : int {
    kUnnecessary = 0,
    kSuccessful = 1,
    kFailureFirmwareFileMultiple = 2,
    kFailureFirmwareFileNotFound = 3,
    kFailureFirmwareFileOpen = 4,
    kFailureFirmwareFileFmap = 5,
    kFailurePreUpdateVersionCheck = 6,
    kFailurePostUpdateVersionCheck = 7,
    kFailureUpdateVersionCheck = 8,
    kFailureUpdateFlashProtect = 9,
    kFailureUpdateRO = 10,
    kFailureUpdateRW = 11,

    kMaxValue = kFailureUpdateRW,
  };

  // Send number of fingers enrolled.
  bool SendEnrolledFingerCount(int finger_count);

  // Is unlocking with FP enabled or not?
  bool SendFpUnlockEnabled(bool enabled);

  // Send matching/capture latency metrics.
  bool SendFpLatencyStats(bool matched,
                          int capture_ms,
                          int match_ms,
                          int overall_ms);

  bool SendFwUpdaterStatus(FwUpdaterStatus status,
                           updater::UpdateReason reason,
                           int overall_ms);

  // Is fingerprint ignored due to parallel power button press?
  bool SendIgnoreMatchEventOnPowerButtonPress(bool is_ignored);

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
