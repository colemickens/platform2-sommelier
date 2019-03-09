// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_ARC_SERVICE_FAILURE_COLLECTOR_H_
#define CRASH_REPORTER_ARC_SERVICE_FAILURE_COLLECTOR_H_

#include "crash-reporter/service_failure_collector.h"

// ARC ServiceFailureCollector.
class ArcServiceFailureCollector : public ServiceFailureCollector {
 public:
  ArcServiceFailureCollector();

  ~ArcServiceFailureCollector() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcServiceFailureCollector);
};

#endif  // CRASH_REPORTER_ARC_SERVICE_FAILURE_COLLECTOR_H_
