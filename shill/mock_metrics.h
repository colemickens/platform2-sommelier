// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_METRICS_
#define SHILL_MOCK_METRICS_

#include <gmock/gmock.h>

#include "shill/metrics.h"

namespace shill {

class MockMetrics : public Metrics {
 public:
  MockMetrics();
  virtual ~MockMetrics();

  MOCK_METHOD1(NotifyDefaultServiceChanged, void(const Service *service));
  MOCK_METHOD2(NotifyServiceStateChanged,
               void(const Service *service, Service::ConnectState new_state));
  MOCK_METHOD3(SendEnumToUMA, bool(const std::string &name, int sample,
                                   int max));
  MOCK_METHOD5(SendToUMA, bool(const std::string &name, int sample, int min,
                               int max, int num_buckets));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMetrics);
};

}

#endif  // SHILL_MOCK_METRICS_
