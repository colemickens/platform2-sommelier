// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/arc_service_failure_collector.h"

ArcServiceFailureCollector::ArcServiceFailureCollector() {
  exec_name_ = "arc-service-failure";
}

ArcServiceFailureCollector::~ArcServiceFailureCollector() {}
