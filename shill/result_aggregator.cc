// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/result_aggregator.h"

#include "shill/logging.h"

namespace shill {

ResultAggregator::ResultAggregator(const ResultCallback &callback)
    : callback_(callback), got_result_(false) {
  CHECK(!callback.is_null());
}

ResultAggregator::~ResultAggregator() {
  if (got_result_) {
    callback_.Run(error_);
  }
}

void ResultAggregator::ReportResult(const Error &error) {
  CHECK(!error.IsOngoing());  // We want the final result.
  got_result_ = true;
  if (error_.IsSuccess()) {  // Only copy first |error|.
    error_.CopyFrom(error);
  }
}

}  // namespace shill
