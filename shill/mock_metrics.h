// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_METRICS_
#define SHILL_MOCK_METRICS_

#include "shill/metrics.h"

namespace shill {

class MockMetrics : public Metrics {
 public:
  MockMetrics();
  virtual ~MockMetrics();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMetrics);
};

}

#endif  // SHILL_MOCK_METRICS_
