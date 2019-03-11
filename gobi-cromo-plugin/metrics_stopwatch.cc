// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi-cromo-plugin/metrics_stopwatch.h"

#include <time.h>

#include <limits>

#include <base/logging.h>
#include <metrics/metrics_library.h>

static const uint64_t kInvalid = std::numeric_limits<uint64_t>::max();

class MetricsLibraryInterface;

MetricsStopwatch::MetricsStopwatch(const char* name,
                                   int min,
                                   int max,
                                   int nbuckets)
    : metrics_(new MetricsLibrary()),
      name_(name),
      min_(min),
      max_(max),
      nbuckets_(nbuckets),
      start_(kInvalid),
      stop_(kInvalid) {}

uint64_t MetricsStopwatch::GetTimeMs() {
  struct timespec ts;
  uint64_t rv;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  rv = ts.tv_sec;
  rv *= 1000;
  rv += ts.tv_nsec / 1000000;
  return rv;
}

void MetricsStopwatch::Reset() {
  start_ = kInvalid;
  stop_ = kInvalid;
}

void MetricsStopwatch::Start() {
  start_ = GetTimeMs();
}

void MetricsStopwatch::Stop() {
  stop_ = GetTimeMs();
  ReportAndReset();
}

void MetricsStopwatch::StopIfStarted() {
  stop_ = GetTimeMs();
  if (start_ != kInvalid) {
    ReportAndReset();
  } else {
    Reset();
  }
}

void MetricsStopwatch::ReportAndReset() {
  if (start_ > stop_ || start_ == kInvalid || stop_ == kInvalid) {
    LOG(ERROR) << "Bad metric: " << start_ << ", " << stop_;
  } else {
    metrics_->SendToUMA(name_, stop_ - start_, min_, max_, nbuckets_);
  }
  Reset();
}

void MetricsStopwatch::SetStart(uint64_t start) {
  start_ = start;
  if (start_ != kInvalid && stop_ != kInvalid) {
    ReportAndReset();
  }
}

void MetricsStopwatch::SetStop(uint64_t stop) {
  stop_ = stop;
  if (start_ != kInvalid && stop_ != kInvalid) {
    ReportAndReset();
  }
}

void MetricsStopwatch::SetMetrics(MetricsLibraryInterface* m) {
  metrics_.reset(m);
}
