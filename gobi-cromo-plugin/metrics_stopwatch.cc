// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metrics_stopwatch.h"

#include <limits.h>
#include <time.h>

#include <base/logging.h>
#include <metrics/metrics_library.h>

static const unsigned long long kInvalid = ULLONG_MAX;

class MetricsLibraryInterface;

MetricsStopwatch::MetricsStopwatch(const char *name,
                                   int min,
                                   int max,
                                   int nbuckets)
    : name_(name),
      min_(min),
      max_(max),
      nbuckets_(nbuckets) {
  Reset();
  metrics_.reset(new MetricsLibrary());
  metrics_->Init();
}

unsigned long long MetricsStopwatch::GetTimeMs(void) {
  struct timespec ts;
  unsigned long long rv;

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

void MetricsStopwatch::SetStart(unsigned long long start) {
  start_ = start;
  if (start_ != kInvalid && stop_ != kInvalid) {
    ReportAndReset();
  }
}

void MetricsStopwatch::SetStop(unsigned long long stop) {
  stop_ = stop;
  if (start_ != kInvalid && stop_ != kInvalid) {
    ReportAndReset();
  }
}

void MetricsStopwatch::SetMetrics(MetricsLibraryInterface *m) {
  metrics_.reset(m);
}
