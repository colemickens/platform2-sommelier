// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ml/request_metrics.h"

#include <metrics/metrics_library.h>

#include "ml/mojom/machine_learning_service.mojom.h"

namespace ml {

using chromeos::machine_learning::mojom::LoadModelResult;

// Records in MachineLearningService.LoadModelResult rather than a
// model-specific enum histogram because the model name is unknown.
void RecordModelSpecificationErrorEvent() {
  MetricsLibrary().SendEnumToUMA(
      "MachineLearningService.LoadModelResult",
      static_cast<int>(LoadModelResult::MODEL_SPEC_ERROR),
      static_cast<int>(LoadModelResult::kMax) + 1);
}

}  // namespace ml
