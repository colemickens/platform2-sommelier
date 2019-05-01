// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/cros_healthd_routine_service_impl.h"

#include <base/logging.h>

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

CrosHealthdRoutineServiceImpl::CrosHealthdRoutineServiceImpl() = default;
CrosHealthdRoutineServiceImpl::~CrosHealthdRoutineServiceImpl() = default;

std::vector<mojo_ipc::DiagnosticRoutineEnum>
CrosHealthdRoutineServiceImpl::GetAvailableRoutines() {
  NOTIMPLEMENTED();
  return std::vector<mojo_ipc::DiagnosticRoutineEnum>();
}

void CrosHealthdRoutineServiceImpl::RunBatteryRoutine(
    int low_mah,
    int high_mah,
    int* id,
    mojo_ipc::DiagnosticRoutineStatusEnum* status) {
  NOTIMPLEMENTED();
}

void CrosHealthdRoutineServiceImpl::RunBatterySysfsRoutine(
    int maximum_cycle_count,
    int percent_battery_wear_allowed,
    int* id,
    mojo_ipc::DiagnosticRoutineStatusEnum* status) {
  NOTIMPLEMENTED();
}

void CrosHealthdRoutineServiceImpl::RunUrandomRoutine(
    int length_seconds,
    int* id,
    mojo_ipc::DiagnosticRoutineStatusEnum* status) {
  NOTIMPLEMENTED();
}

void CrosHealthdRoutineServiceImpl::GetRoutineUpdate(
    int32_t uuid,
    mojo_ipc::DiagnosticRoutineCommandEnum command,
    bool include_output,
    const mojo_ipc::CrosHealthdService::GetRoutineUpdateCallback& callback) {
  NOTIMPLEMENTED();
}

}  // namespace diagnostics
