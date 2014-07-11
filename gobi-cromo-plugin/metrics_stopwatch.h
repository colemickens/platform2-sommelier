// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOBI_CROMO_PLUGIN_METRICS_STOPWATCH_H_
#define GOBI_CROMO_PLUGIN_METRICS_STOPWATCH_H_

#include <stdint.h>

#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <metrics/metrics_library.h>

class MetricsStopwatch {
 public:
  MetricsStopwatch(const char* name, int min, int max, int nbuckets);

  void Start();
  void Stop();  // and send measurement to UMA and reset
  void StopIfStarted();  // and send measurement to UMA and reset

  // In some situations, we can receive a report of the start time
  // after the stop time.  These report when both start and stop are
  // known.
  void SetStart(uint64_t start);
  void SetStop(uint64_t stop);

  void Reset();  // Only call this to abandon a measurement; Stop*()
                 // and Set*() already call this automatically.

  static uint64_t GetTimeMs(void);

  void SetMetrics(MetricsLibraryInterface* m);

 protected:
  void ReportAndReset();

  scoped_ptr<MetricsLibraryInterface> metrics_;
  std::string name_;
  int min_;
  int max_;
  int nbuckets_;
  uint64_t start_;
  uint64_t stop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsStopwatch);
};

#endif  // GOBI_CROMO_PLUGIN_METRICS_STOPWATCH_H_
