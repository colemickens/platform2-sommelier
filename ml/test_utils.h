// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ML_TEST_UTILS_H_
#define ML_TEST_UTILS_H_

#include <string>
#include <vector>

#include "ml/mojom/tensor.mojom.h"
#include "ml/tensor_view.h"

namespace ml {

// Create a tensor with the given shape and values. Does no validity checking
// (by design, as we sometimes need to pass bad tensors to test error handling).
template <typename T>
chromeos::machine_learning::mojom::TensorPtr NewTensor(
    const std::vector<int64_t>& shape, const std::vector<T>& values) {
  auto tensor(chromeos::machine_learning::mojom::Tensor::New());
  TensorView<T> tensor_view(tensor);
  tensor_view.Allocate();
  tensor_view.GetShape() = shape;
  tensor_view.GetValues() = values;

  return tensor;
}

// Return the model directory for tests (or die if it cannot be obtained).
std::string GetTestModelDir();

}  // namespace ml

#endif  // ML_TEST_UTILS_H_
