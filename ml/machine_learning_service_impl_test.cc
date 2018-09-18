// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <base/run_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <mojo/public/cpp/bindings/interface_request.h>

#include "ml/machine_learning_service_impl.h"
#include "ml/mojom/graph_executor.mojom.h"
#include "ml/mojom/machine_learning_service.mojom.h"
#include "ml/mojom/model.mojom.h"
#include "ml/tensor_view.h"
#include "ml/test_utils.h"

namespace ml {
namespace {

using ::chromeos::machine_learning::mojom::CreateGraphExecutorResult;
using ::chromeos::machine_learning::mojom::ExecuteResult;
using ::chromeos::machine_learning::mojom::GraphExecutorPtr;
using ::chromeos::machine_learning::mojom::LoadModelResult;
using ::chromeos::machine_learning::mojom::MachineLearningServicePtr;
using ::chromeos::machine_learning::mojom::Model;
using ::chromeos::machine_learning::mojom::ModelId;
using ::chromeos::machine_learning::mojom::ModelPtr;
using ::chromeos::machine_learning::mojom::ModelRequest;
using ::chromeos::machine_learning::mojom::ModelSpec;
using ::chromeos::machine_learning::mojom::ModelSpecPtr;
using ::chromeos::machine_learning::mojom::TensorPtr;
using ::testing::DoubleEq;
using ::testing::ElementsAre;

// A version of MachineLearningServiceImpl that loads from the testing model
// directory.
class MachineLearningServiceImplForTesting : public MachineLearningServiceImpl {
 public:
  // Pass a dummy callback and use the testing model directory.
  explicit MachineLearningServiceImplForTesting(
      mojo::ScopedMessagePipeHandle pipe)
      : MachineLearningServiceImpl(
            std::move(pipe), base::Closure(), GetTestModelDir()) {}
};

TEST(MachineLearningServiceImplTest, TestBadModel) {
  MachineLearningServicePtr ml_service;
  const MachineLearningServiceImplForTesting ml_service_impl(
      mojo::GetProxy(&ml_service).PassMessagePipe());

  // Set up model spec to specify an invalid model.
  ModelSpecPtr spec = ModelSpec::New();
  spec->id = ModelId::UNKNOWN;

  // Load model.
  ModelPtr model;
  bool model_callback_done = false;
  ml_service->LoadModel(std::move(spec), mojo::GetProxy(&model),
                        [&model_callback_done](const LoadModelResult result) {
                          EXPECT_EQ(result, LoadModelResult::MODEL_SPEC_ERROR);
                          model_callback_done = true;
                        });
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(model_callback_done);
}

TEST(MachineLearningServiceImplTest, TestInference) {
  MachineLearningServicePtr ml_service;
  const MachineLearningServiceImplForTesting ml_service_impl(
      mojo::GetProxy(&ml_service).PassMessagePipe());

  // Set up model spec.
  ModelSpecPtr spec = ModelSpec::New();
  spec->id = ModelId::TEST_MODEL;

  // Load model.
  ModelPtr model;
  bool model_callback_done = false;
  ml_service->LoadModel(std::move(spec), mojo::GetProxy(&model),
                        [&model_callback_done](const LoadModelResult result) {
                          EXPECT_EQ(result, LoadModelResult::OK);
                          model_callback_done = true;
                        });
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(model_callback_done);
  ASSERT_TRUE(model.is_bound());

  // Get graph executor.
  GraphExecutorPtr graph_executor;
  bool ge_callback_done = false;
  model->CreateGraphExecutor(
      mojo::GetProxy(&graph_executor),
      [&ge_callback_done](const CreateGraphExecutorResult result) {
        EXPECT_EQ(result, CreateGraphExecutorResult::OK);
        ge_callback_done = true;
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(ge_callback_done);
  ASSERT_TRUE(graph_executor.is_bound());

  // Construct input.
  mojo::Map<mojo::String, TensorPtr> inputs;
  inputs.insert("x", NewTensor<double>({1}, {0.5}));
  inputs.insert("y", NewTensor<double>({1}, {0.25}));
  mojo::Array<mojo::String> outputs({"z"});

  // Perform inference.
  bool infer_callback_done = false;
  graph_executor->Execute(
      std::move(inputs), std::move(outputs),
      [&infer_callback_done](const ExecuteResult result,
                             const mojo::Array<TensorPtr> outputs) {
        // Check that the inference succeeded and gives the expected number of
        // outputs.
        EXPECT_EQ(result, ExecuteResult::OK);
        ASSERT_EQ(outputs.size(), 1);

        // Check that the output tensor has the right type and format.
        const TensorView<double> out_tensor(outputs[0]);
        EXPECT_TRUE(out_tensor.IsValidType());
        EXPECT_TRUE(out_tensor.IsValidFormat());

        // Check the output tensor has the expected shape and values.
        EXPECT_THAT(out_tensor.GetShape(), ElementsAre(1));
        EXPECT_THAT(out_tensor.GetValues(), ElementsAre(DoubleEq(0.75)));

        infer_callback_done = true;
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(infer_callback_done);
}

}  // namespace
}  // namespace ml
