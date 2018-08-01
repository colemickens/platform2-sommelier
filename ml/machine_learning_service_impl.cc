// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ml/machine_learning_service_impl.h"

#include <utility>

namespace ml {

MachineLearningServiceImpl::MachineLearningServiceImpl(
    mojo::ScopedMessagePipeHandle pipe, base::Closure connection_error_handler)
    : binding_(this, std::move(pipe)) {
  binding_.set_connection_error_handler(std::move(connection_error_handler));
}

void MachineLearningServiceImpl::LoadModel(
    chromeos::machine_learning::mojom::ModelSpecPtr spec,
    chromeos::machine_learning::mojom::ModelRequest request) {
  NOTIMPLEMENTED();
}

}  // namespace ml
