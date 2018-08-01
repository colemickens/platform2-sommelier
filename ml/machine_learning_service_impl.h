// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ML_MACHINE_LEARNING_SERVICE_IMPL_H_
#define ML_MACHINE_LEARNING_SERVICE_IMPL_H_

#include <base/callback_forward.h>
#include <base/macros.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "mojom/machine_learning_service.mojom.h"

namespace ml {

class MachineLearningServiceImpl
    : public chromeos::machine_learning::mojom::MachineLearningService {
 public:
  // Creates an instance bound to |pipe|. The specified
  // |connection_error_handler| will be invoked if the binding encounters a
  // connection error.
  MachineLearningServiceImpl(mojo::ScopedMessagePipeHandle pipe,
                             base::Closure connection_error_handler);

 private:
  // chromeos::machine_learning::mojom::MachineLearningService:
  void LoadModel(
      chromeos::machine_learning::mojom::ModelSpecPtr spec,
      chromeos::machine_learning::mojom::ModelRequest request) override;

  mojo::Binding<chromeos::machine_learning::mojom::MachineLearningService>
      binding_;

  DISALLOW_COPY_AND_ASSIGN(MachineLearningServiceImpl);
};

}  // namespace ml

#endif  // ML_MACHINE_LEARNING_SERVICE_IMPL_H_
