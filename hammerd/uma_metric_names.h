// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HAMMERD_UMA_METRIC_NAMES_H_
#define HAMMERD_UMA_METRIC_NAMES_H_

namespace hammerd {

#define DETACHABLE_BASE_PREFIX "DetachableBase."

const char kMetricROUpdateResult[] = DETACHABLE_BASE_PREFIX "ROUpdateResult";
const char kMetricRWUpdateResult[] = DETACHABLE_BASE_PREFIX "RWUpdateResult";
const char kMetricPairResult[] = DETACHABLE_BASE_PREFIX "PairResult";

enum class ROUpdateResult {
  kSucceeded = 1,
  kTransferFailed,

  kMax,
};

enum class RWUpdateResult {
  kSucceeded = 1,
  kTransferFailed,
  kInvalidKey,
  kRollbackDisallowed,

  kMax,
};

enum class PairResult {
  kUnknownError = 0,
  kChallengePassed = 1,
  kChallengeFailed,
  kNeedInjectEntropy,

  kMax,
};

}  // namespace hammerd
#endif  // HAMMERD_UMA_METRIC_NAMES_H_
