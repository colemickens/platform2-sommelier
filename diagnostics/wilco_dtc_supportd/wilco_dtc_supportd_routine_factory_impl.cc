// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_routine_factory_impl.h"

#include <base/logging.h>

#include "diagnostics/routines/battery/battery.h"
#include "diagnostics/routines/battery_sysfs/battery_sysfs.h"
#include "diagnostics/routines/smartctl_check/smartctl_check.h"
#include "diagnostics/routines/urandom/urandom.h"

namespace diagnostics {

WilcoDtcSupportdRoutineFactoryImpl::WilcoDtcSupportdRoutineFactoryImpl() =
    default;
WilcoDtcSupportdRoutineFactoryImpl::~WilcoDtcSupportdRoutineFactoryImpl() =
    default;

std::unique_ptr<DiagnosticRoutine>
WilcoDtcSupportdRoutineFactoryImpl::CreateRoutine(
    const grpc_api::RunRoutineRequest& request) {
  switch (request.routine()) {
    case grpc_api::ROUTINE_BATTERY:
      DCHECK_EQ(request.parameters_case(),
                grpc_api::RunRoutineRequest::kBatteryParams);
      return std::make_unique<BatteryRoutine>(request.battery_params());
    case grpc_api::ROUTINE_BATTERY_SYSFS:
      return std::make_unique<BatterySysfsRoutine>(
          request.battery_sysfs_params());
    case grpc_api::ROUTINE_URANDOM:
      DCHECK_EQ(request.parameters_case(),
                grpc_api::RunRoutineRequest::kUrandomParams);
      return std::make_unique<UrandomRoutine>(request.urandom_params());
    case grpc_api::ROUTINE_SMARTCTL_CHECK:
      return std::make_unique<SmartctlCheckRoutine>(
          request.smartctl_check_params());
    default:
      LOG(ERROR) << "RunRoutineRequest routine not set or unrecognized.";
      return nullptr;
  }
}

}  // namespace diagnostics
