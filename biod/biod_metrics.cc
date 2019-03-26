// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/biod_metrics.h"

#include <metrics/metrics_library.h>

namespace biod {

namespace metrics {

constexpr char kFpUnlockEnabled[] = "Fingerprint.UnlockEnabled";
constexpr char kFpEnrolledFingerCount[] =
    "Fingerprint.Unlock.EnrolledFingerCount";
constexpr char kFpMatchDurationCapture[] =
    "Fingerprint.Unlock.Match.Duration.Capture";
constexpr char kFpMatchDurationMatcher[] =
    "Fingerprint.Unlock.Match.Duration.Matcher";
constexpr char kFpMatchDurationOverall[] =
    "Fingerprint.Unlock.Match.Duration.Overall";
constexpr char kFpNoMatchDurationCapture[] =
    "Fingerprint.Unlock.NoMatch.Duration.Capture";
constexpr char kFpNoMatchDurationMatcher[] =
    "Fingerprint.Unlock.NoMatch.Duration.Matcher";
constexpr char kFpNoMatchDurationOverall[] =
    "Fingerprint.Unlock.NoMatch.Duration.Overall";
constexpr char kFpMatchIgnoredDueToPowerButtonPress[] =
    "Fingerprint.Unlock.MatchIgnoredDueToPowerButtonPress";

}  // namespace metrics

BiodMetrics::BiodMetrics() : metrics_lib_(std::make_unique<MetricsLibrary>()) {}

bool BiodMetrics::SendEnrolledFingerCount(int finger_count) {
  return metrics_lib_->SendEnumToUMA(metrics::kFpEnrolledFingerCount,
                                     finger_count, 10);
}

bool BiodMetrics::SendFpUnlockEnabled(bool enabled) {
  return metrics_lib_->SendBoolToUMA(metrics::kFpUnlockEnabled, enabled);
}

bool BiodMetrics::SendFpLatencyStats(bool matched,
                                     int capture_ms,
                                     int match_ms,
                                     int overall_ms) {
  bool rc = true;
  rc = metrics_lib_->SendToUMA(matched ? metrics::kFpMatchDurationCapture
                                       : metrics::kFpNoMatchDurationCapture,
                               capture_ms, 0, 200, 20) &&
       rc;
  rc = metrics_lib_->SendToUMA(matched ? metrics::kFpMatchDurationMatcher
                                       : metrics::kFpNoMatchDurationMatcher,
                               match_ms, 100, 800, 50) &&
       rc;
  rc = metrics_lib_->SendToUMA(matched ? metrics::kFpMatchDurationOverall
                                       : metrics::kFpNoMatchDurationOverall,
                               overall_ms, 100, 1000, 50) &&
       rc;
  return rc;
}

bool BiodMetrics::SendIgnoreMatchEventOnPowerButtonPress(bool is_ignored) {
  return metrics_lib_->SendBoolToUMA(
      metrics::kFpMatchIgnoredDueToPowerButtonPress, is_ignored);
}

void BiodMetrics::SetMetricsLibraryForTesting(
    std::unique_ptr<MetricsLibraryInterface> metrics_lib) {
  metrics_lib_ = std::move(metrics_lib);
}

}  // namespace biod
