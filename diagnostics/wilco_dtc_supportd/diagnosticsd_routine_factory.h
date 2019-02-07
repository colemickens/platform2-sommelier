// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_DIAGNOSTICSD_ROUTINE_FACTORY_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_DIAGNOSTICSD_ROUTINE_FACTORY_H_

#include <memory>

#include "diagnostics/routines/diag_routine.h"
#include "diagnosticsd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Interface for constructing DiagnosticRoutines from RunRoutineRequests.
class DiagnosticsdRoutineFactory {
 public:
  virtual ~DiagnosticsdRoutineFactory() = default;

  virtual std::unique_ptr<DiagnosticRoutine> CreateRoutine(
      const grpc_api::RunRoutineRequest& request) = 0;
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_DIAGNOSTICSD_ROUTINE_FACTORY_H_
