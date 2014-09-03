// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_RESULT_AGGREGATOR_H_
#define SHILL_RESULT_AGGREGATOR_H_

#include <base/macros.h>
#include <base/memory/ref_counted.h>

#include "shill/callbacks.h"
#include "shill/error.h"

namespace shill {

// The ResultAggregator is used to aggregate the result of multiple
// asynchronous operations. To use: construct a ResultAggregator, and
// Bind its ReportResult methods to some Callbacks.
//
// When the Callbacks are destroyed, they will drop their references
// to the ResultAggregator. When all references to the
// ResultAggregator are destroyed, the ResultAggregator will invoke
// the |callback| with which ResultAggregator was constructed. However,
// if no Callbacks invoked ReportResult, then the original |callback|
// will not be invoked.
//
// |callback| will see Error type of Success if all Callbacks reported
// Success to ResultAggregator. Otherwise, |callback| will see the first
// of the Errors reported to ResultAggregator.
class ResultAggregator : public base::RefCounted<ResultAggregator> {
 public:
  explicit ResultAggregator(const ResultCallback &callback);
  virtual ~ResultAggregator();

  void ReportResult(const Error &error);

 private:
  const ResultCallback callback_;
  bool got_result_;
  Error error_;

  DISALLOW_COPY_AND_ASSIGN(ResultAggregator);
};

}  // namespace shill

#endif  // SHILL_RESULT_AGGREGATOR_H_
