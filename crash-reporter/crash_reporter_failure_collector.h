// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_CRASH_REPORTER_FAILURE_COLLECTOR_H_
#define CRASH_REPORTER_CRASH_REPORTER_FAILURE_COLLECTOR_H_

#include <base/macros.h>

#include "crash-reporter/crash_collector.h"

// Collector to record crash_reportor itself crashing.
class CrashReporterFailureCollector : public CrashCollector {
 public:
  CrashReporterFailureCollector();

  ~CrashReporterFailureCollector() override;

  // Collect crash reporter failures.
  void Collect();

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashReporterFailureCollector);
};

#endif  // CRASH_REPORTER_CRASH_REPORTER_FAILURE_COLLECTOR_H_
