// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_FAKE_DIAGNOSTICSD_ROUTINE_FACTORY_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_FAKE_DIAGNOSTICSD_ROUTINE_FACTORY_H_

#include <memory>

#include <base/macros.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/routines/diag_routine.h"
#include "diagnostics/wilco_dtc_supportd/diagnosticsd_routine_factory.h"
#include "diagnosticsd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Implementation of DiagnosticsdRoutineFactory that should only be used for
// testing.
class FakeDiagnosticsdRoutineFactory final : public DiagnosticsdRoutineFactory {
 public:
  FakeDiagnosticsdRoutineFactory();

  // DiagnosticsdRoutineFactory overrides:
  ~FakeDiagnosticsdRoutineFactory() override;
  std::unique_ptr<DiagnosticRoutine> CreateRoutine(
      const grpc_api::RunRoutineRequest& request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDiagnosticsdRoutineFactory);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_FAKE_DIAGNOSTICSD_ROUTINE_FACTORY_H_
