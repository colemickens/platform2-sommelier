// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/fake_routine_factory.h"

#include <cstdint>
#include <utility>

#include "diagnostics/common/mojo_utils.h"
#include "mojo/cros_healthd_diagnostics.mojom.h"

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

namespace {
class FakeDiagnosticRoutine : public DiagnosticRoutine {
 public:
  FakeDiagnosticRoutine(mojo_ipc::DiagnosticRoutineStatusEnum status,
                        uint32_t progress_percent,
                        const std::string& output);
  // DiagnosticRoutine overrides:
  ~FakeDiagnosticRoutine() override;
  void Start() override;
  void Resume() override;
  void Cancel() override;
  void PopulateStatusUpdate(mojo_ipc::RoutineUpdate* response,
                            bool include_output) override;
  mojo_ipc::DiagnosticRoutineStatusEnum GetStatus() override;

 private:
  const mojo_ipc::DiagnosticRoutineStatusEnum status_;
  const uint32_t progress_percent_;
  const std::string output_;

  DISALLOW_COPY_AND_ASSIGN(FakeDiagnosticRoutine);
};

class InteractiveFakeDiagnosticRoutine final : public FakeDiagnosticRoutine {
 public:
  InteractiveFakeDiagnosticRoutine(
      mojo_ipc::DiagnosticRoutineUserMessageEnum user_message,
      uint32_t progress_percent,
      const std::string& output);
  ~InteractiveFakeDiagnosticRoutine() override;

  // DiagnosticRoutine overrides:
  void PopulateStatusUpdate(mojo_ipc::RoutineUpdate* response,
                            bool include_output) override;

 private:
  const mojo_ipc::DiagnosticRoutineUserMessageEnum user_message_;

  DISALLOW_COPY_AND_ASSIGN(InteractiveFakeDiagnosticRoutine);
};

class NonInteractiveFakeDiagnosticRoutine final : public FakeDiagnosticRoutine {
 public:
  NonInteractiveFakeDiagnosticRoutine(
      mojo_ipc::DiagnosticRoutineStatusEnum status,
      const std::string& status_message,
      uint32_t progress_percent,
      const std::string& output);
  ~NonInteractiveFakeDiagnosticRoutine() override;

  // DiagnosticRoutine overrides:
  void PopulateStatusUpdate(mojo_ipc::RoutineUpdate* response,
                            bool include_output) override;

 private:
  const std::string status_message_;

  DISALLOW_COPY_AND_ASSIGN(NonInteractiveFakeDiagnosticRoutine);
};

FakeDiagnosticRoutine::FakeDiagnosticRoutine(
    mojo_ipc::DiagnosticRoutineStatusEnum status,
    uint32_t progress_percent,
    const std::string& output)
    : status_(status), progress_percent_(progress_percent), output_(output) {}

FakeDiagnosticRoutine::~FakeDiagnosticRoutine() = default;

void FakeDiagnosticRoutine::Start() {}
void FakeDiagnosticRoutine::Resume() {}
void FakeDiagnosticRoutine::Cancel() {}

void FakeDiagnosticRoutine::PopulateStatusUpdate(
    mojo_ipc::RoutineUpdate* response, bool include_output) {
  DCHECK(response);

  response->progress_percent = progress_percent_;

  if (output_.empty())
    return;

  response->output = CreateReadOnlySharedMemoryMojoHandle(output_);
}

mojo_ipc::DiagnosticRoutineStatusEnum FakeDiagnosticRoutine::GetStatus() {
  return status_;
}

InteractiveFakeDiagnosticRoutine::InteractiveFakeDiagnosticRoutine(
    mojo_ipc::DiagnosticRoutineUserMessageEnum user_message,
    uint32_t progress_percent,
    const std::string& output)
    : FakeDiagnosticRoutine(mojo_ipc::DiagnosticRoutineStatusEnum::kReady,
                            progress_percent,
                            output),
      user_message_(user_message) {}

InteractiveFakeDiagnosticRoutine::~InteractiveFakeDiagnosticRoutine() = default;

void InteractiveFakeDiagnosticRoutine::PopulateStatusUpdate(
    mojo_ipc::RoutineUpdate* response, bool include_output) {
  FakeDiagnosticRoutine::PopulateStatusUpdate(response, include_output);
  mojo_ipc::InteractiveRoutineUpdate update;
  update.user_message = user_message_;
  response->routine_update_union->set_interactive_update(update.Clone());
}

NonInteractiveFakeDiagnosticRoutine::NonInteractiveFakeDiagnosticRoutine(
    mojo_ipc::DiagnosticRoutineStatusEnum status,
    const std::string& status_message,
    uint32_t progress_percent,
    const std::string& output)
    : FakeDiagnosticRoutine(status, progress_percent, output),
      status_message_(status_message) {}

NonInteractiveFakeDiagnosticRoutine::~NonInteractiveFakeDiagnosticRoutine() =
    default;

void NonInteractiveFakeDiagnosticRoutine::PopulateStatusUpdate(
    mojo_ipc::RoutineUpdate* response, bool include_output) {
  FakeDiagnosticRoutine::PopulateStatusUpdate(response, include_output);
  mojo_ipc::NonInteractiveRoutineUpdate update;
  update.status = GetStatus();
  update.status_message = status_message_;
  response->routine_update_union->set_noninteractive_update(update.Clone());
}

}  // namespace

FakeRoutineFactory::FakeRoutineFactory() = default;
FakeRoutineFactory::~FakeRoutineFactory() = default;

void FakeRoutineFactory::SetInteractiveStatus(
    mojo_ipc::DiagnosticRoutineUserMessageEnum user_message,
    uint32_t progress_percent,
    const std::string& output) {
  next_routine_ = std::make_unique<InteractiveFakeDiagnosticRoutine>(
      user_message, progress_percent, output);
}

void FakeRoutineFactory::SetNonInteractiveStatus(
    mojo_ipc::DiagnosticRoutineStatusEnum status,
    const std::string& status_message,
    uint32_t progress_percent,
    const std::string& output) {
  next_routine_ = std::make_unique<NonInteractiveFakeDiagnosticRoutine>(
      status, status_message, progress_percent, output);
}

std::unique_ptr<DiagnosticRoutine> FakeRoutineFactory::CreateRoutine(
    const grpc_api::RunRoutineRequest& request) {
  return std::move(next_routine_);
}

}  // namespace diagnostics
