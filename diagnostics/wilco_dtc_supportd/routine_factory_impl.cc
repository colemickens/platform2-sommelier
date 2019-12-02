// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/routine_factory_impl.h"

#include <base/logging.h>

#include "diagnostics/routines/battery/battery.h"
#include "diagnostics/routines/battery_sysfs/battery_sysfs.h"
#include "diagnostics/routines/smartctl_check/smartctl_check.h"
#include "diagnostics/routines/urandom/urandom.h"

namespace diagnostics {

namespace {
// When the battery routine's low_mah and high_mah parameters are not set, we
// default to low_mah = 1000 and high_mah = 10000.
constexpr int kRoutineBatteryDefaultLowmAh = 1000;
constexpr int kRoutineBatteryDefaultHighmAh = 10000;
}  // namespace

RoutineFactoryImpl::RoutineFactoryImpl() = default;
RoutineFactoryImpl::~RoutineFactoryImpl() = default;

std::unique_ptr<DiagnosticRoutine> RoutineFactoryImpl::CreateRoutine(
    const grpc_api::RunRoutineRequest& request) {
  switch (request.routine()) {
    case grpc_api::ROUTINE_BATTERY:
      DCHECK_EQ(request.parameters_case(),
                grpc_api::RunRoutineRequest::kBatteryParams);
      return std::make_unique<BatteryRoutine>(
          request.battery_params().low_mah()
              ? request.battery_params().low_mah()
              : kRoutineBatteryDefaultLowmAh,
          request.battery_params().high_mah()
              ? request.battery_params().high_mah()
              : kRoutineBatteryDefaultHighmAh);
    case grpc_api::ROUTINE_BATTERY_SYSFS:
      return std::make_unique<BatterySysfsRoutine>(
          request.battery_sysfs_params().maximum_cycle_count(),
          request.battery_sysfs_params().percent_battery_wear_allowed());
    case grpc_api::ROUTINE_URANDOM:
      DCHECK_EQ(request.parameters_case(),
                grpc_api::RunRoutineRequest::kUrandomParams);
      return CreateUrandomRoutine(request.urandom_params().length_seconds());
    case grpc_api::ROUTINE_SMARTCTL_CHECK:
      return CreateSmartctlCheckRoutine();
    default:
      LOG(ERROR) << "RunRoutineRequest routine not set or unrecognized.";
      return nullptr;
  }
}

}  // namespace diagnostics
