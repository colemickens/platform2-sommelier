// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/cros_healthd_mojo_service.h"

#include <string>
#include <utility>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <dbus/cros_healthd/dbus-constants.h>
#include <mojo/edk/embedder/embedder.h>

#include "diagnostics/cros_healthd/utils/battery_utils.h"
#include "diagnostics/cros_healthd/utils/disk_utils.h"

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

CrosHealthdMojoService::CrosHealthdMojoService(
    CrosHealthdRoutineService* routine_service,
    mojo::ScopedMessagePipeHandle mojo_pipe_handle)
    : routine_service_(routine_service),
      self_binding_(this /* impl */, std::move(mojo_pipe_handle)) {
  DCHECK(routine_service_);
  DCHECK(self_binding_.is_bound());
}

CrosHealthdMojoService::~CrosHealthdMojoService() = default;

void CrosHealthdMojoService::GetAvailableRoutines(
    const GetAvailableRoutinesCallback& callback) {
  NOTIMPLEMENTED();
}

void CrosHealthdMojoService::RunRoutine(mojo_ipc::DiagnosticRoutineEnum routine,
                                        mojo_ipc::RoutineParametersPtr params,
                                        const RunRoutineCallback& callback) {
  NOTIMPLEMENTED();
}

void CrosHealthdMojoService::GetRoutineUpdate(
    int32_t uuid,
    mojo_ipc::DiagnosticRoutineCommandEnum command,
    bool include_output,
    const GetRoutineUpdateCallback& callback) {
  NOTIMPLEMENTED();
}

void CrosHealthdMojoService::set_connection_error_handler(
    const base::Closure& error_handler) {
  self_binding_.set_connection_error_handler(error_handler);
}

void CrosHealthdMojoService::ProbeBatteryInfo(
    const ProbeBatteryInfoCallback& callback) {
  callback.Run(FetchBatteryInfo());
}

void CrosHealthdMojoService::ProbeNonRemovableBlockDeviceInfo(
    const ProbeNonRemovableBlockDeviceInfoCallback& callback) {
  // Gather various info on non-removeable block devices.
  callback.Run(
      disk_utils::FetchNonRemovableBlockDevicesInfo(base::FilePath("/")));
}

void CrosHealthdMojoService::ProbeCachedVpdInfo(
    const ProbeCachedVpdInfoCallback& callback) {
  callback.Run(disk_utils::FetchCachedVpdInfo(base::FilePath("/")));
}

}  // namespace diagnostics
