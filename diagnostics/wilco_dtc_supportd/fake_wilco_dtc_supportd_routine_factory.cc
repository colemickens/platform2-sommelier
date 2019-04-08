// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/fake_wilco_dtc_supportd_routine_factory.h"

namespace diagnostics {

namespace {
class FakeDiagnosticRoutine final : public DiagnosticRoutine {
 public:
  FakeDiagnosticRoutine();
  // DiagnosticRoutine overrides:
  ~FakeDiagnosticRoutine() override;
  void Start() override;
  void Resume() override;
  void Cancel() override;
  void PopulateStatusUpdate(grpc_api::GetRoutineUpdateResponse* response,
                            bool include_output) override;
  grpc_api::DiagnosticRoutineStatus GetStatus() override;

 private:
  grpc_api::DiagnosticRoutineStatus status_;

  DISALLOW_COPY_AND_ASSIGN(FakeDiagnosticRoutine);
};
}  // namespace

FakeWilcoDtcSupportdRoutineFactory::FakeWilcoDtcSupportdRoutineFactory() =
    default;
FakeWilcoDtcSupportdRoutineFactory::~FakeWilcoDtcSupportdRoutineFactory() =
    default;

std::unique_ptr<DiagnosticRoutine>
FakeWilcoDtcSupportdRoutineFactory::CreateRoutine(
    const grpc_api::RunRoutineRequest& request) {
  return std::make_unique<FakeDiagnosticRoutine>();
}

FakeDiagnosticRoutine::FakeDiagnosticRoutine() = default;
FakeDiagnosticRoutine::~FakeDiagnosticRoutine() = default;

void FakeDiagnosticRoutine::Start() {
  status_ = grpc_api::ROUTINE_STATUS_RUNNING;
}

void FakeDiagnosticRoutine::Resume() {
  status_ = grpc_api::ROUTINE_STATUS_RUNNING;
}

void FakeDiagnosticRoutine::Cancel() {
  status_ = grpc_api::ROUTINE_STATUS_CANCELLED;
}

void FakeDiagnosticRoutine::PopulateStatusUpdate(
    grpc_api::GetRoutineUpdateResponse* response, bool include_output) {
  response->set_status(status_);
}

grpc_api::DiagnosticRoutineStatus FakeDiagnosticRoutine::GetStatus() {
  return status_;
}

}  // namespace diagnostics
