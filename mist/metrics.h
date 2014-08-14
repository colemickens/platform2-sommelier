// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_METRICS_H_
#define MIST_METRICS_H_

#include <base/basictypes.h>
#include <metrics/metrics_library.h>

namespace mist {

// A class for collecting mist related UMA metrics.
class Metrics {
 public:
  Metrics();
  ~Metrics() = default;

  void RecordSwitchResult(bool success);

 private:
  MetricsLibrary metrics_library_;

  DISALLOW_COPY_AND_ASSIGN(Metrics);
};

}  // namespace mist

#endif  // MIST_METRICS_H_
