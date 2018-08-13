// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ml/model_metadata.h"

namespace ml {

using ::chromeos::machine_learning::mojom::ModelId;

std::map<ModelId, ModelMetadata> GetModelMetadata() {
  return {
    {
      ModelId::TEST_MODEL, {
        ModelId::TEST_MODEL,
        "mlservice-model-tab_discarder-quantized-20180704.tflite",
        {{"input", 4}},
        {{"output", 5}},
      },
    },
  };
}

}  // namespace ml
