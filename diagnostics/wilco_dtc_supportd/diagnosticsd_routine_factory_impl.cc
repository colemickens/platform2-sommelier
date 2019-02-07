// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/diagnosticsd_routine_factory_impl.h"

#include <base/logging.h>

#include "diagnostics/routines/battery/battery.h"
#include "diagnostics/routines/urandom/urandom.h"

namespace diagnostics {

DiagnosticsdRoutineFactoryImpl::DiagnosticsdRoutineFactoryImpl() = default;
DiagnosticsdRoutineFactoryImpl::~DiagnosticsdRoutineFactoryImpl() = default;

std::unique_ptr<DiagnosticRoutine>
DiagnosticsdRoutineFactoryImpl::CreateRoutine(
    const grpc_api::RunRoutineRequest& request) {
  switch (request.parameters_case()) {
    case grpc_api::RunRoutineRequest::kBatteryParams:
      return std::make_unique<BatteryRoutine>(request.battery_params());
    case grpc_api::RunRoutineRequest::kUrandomParams:
      return std::make_unique<UrandomRoutine>(request.urandom_params());
    case grpc_api::RunRoutineRequest::PARAMETERS_NOT_SET:
      LOG(ERROR) << "RunRoutineRequest parameters not set or unrecognized.";
      return nullptr;
  }
}

}  // namespace diagnostics
