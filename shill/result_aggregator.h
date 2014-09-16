// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_RESULT_AGGREGATOR_H_
#define SHILL_RESULT_AGGREGATOR_H_

#include <base/cancelable_callback.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>

#include "shill/callbacks.h"
#include "shill/error.h"

namespace shill {

class EventDispatcher;

// The ResultAggregator is used to aggregate the result of multiple
// asynchronous operations. To use: construct a ResultAggregator, and
// Bind its ReportResult methods to some Callbacks. The ResultAggregator
// can also be constructed with an EventDispatcher pointer and timeout delay if
// we want to wait for a limited period of time for asynchronous operations
// to complete.
//
// When the Callbacks are destroyed, they will drop their references
// to the ResultAggregator. When all references to the
// ResultAggregator are destroyed, or if a timeout occurs, the ResultAggregator
// will invoke the |callback| with which ResultAggregator was constructed.
// |callback| will only be invoked exactly once by whichever of these two
// events occurs first. However, if no callbacks invoked ReportResult, then the
// original |callback| will not be invoked.
//
// |callback| will see Error type of Success if all Callbacks reported
// Success to ResultAggregator. If the timeout occurs, |callback| will see Error
// type of OperationTimeout. Otherwise, |callback| will see the first of the
// Errors reported to ResultAggregator.
class ResultAggregator : public base::RefCounted<ResultAggregator> {
 public:
  explicit ResultAggregator(const ResultCallback &callback);
  ResultAggregator(const ResultCallback &callback, EventDispatcher *dispatcher,
                   int timeout_milliseconds);
  virtual ~ResultAggregator();

  void ReportResult(const Error &error);

 private:
  // Callback for timeout registered with EventDispatcher.
  void Timeout();

  base::WeakPtrFactory<ResultAggregator> weak_ptr_factory_;
  const ResultCallback callback_;
  base::CancelableClosure timeout_callback_;
  bool got_result_;
  bool timed_out_;
  Error error_;

  DISALLOW_COPY_AND_ASSIGN(ResultAggregator);
};

}  // namespace shill

#endif  // SHILL_RESULT_AGGREGATOR_H_
