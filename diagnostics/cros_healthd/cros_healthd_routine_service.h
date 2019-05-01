// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_ROUTINE_SERVICE_H_
#define DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_ROUTINE_SERVICE_H_

#include <vector>

#include "mojo/cros_healthd.mojom.h"

namespace diagnostics {

// Service responsible for controlling and managing the lifecycle of diagnostic
// routines.
class CrosHealthdRoutineService {
 public:
  using MojomCrosHealthdDiagnosticRoutineEnum =
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum;
  using MojomCrosHealthdDiagnosticRoutineCommandEnum =
      chromeos::cros_healthd::mojom::DiagnosticRoutineCommandEnum;
  using MojomCrosHealthdDiagnosticRoutineStatusEnum =
      chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum;

  virtual ~CrosHealthdRoutineService() = default;

  // Fetch all of the routines that the device supports.
  virtual std::vector<MojomCrosHealthdDiagnosticRoutineEnum>
  GetAvailableRoutines() = 0;

  // Each of the following methods creates a new instance of the specified
  // diagnostic routine, and starts that instance. See
  // diagnostics/mojo/cros_healthd_diagnostics.mojom for details on each
  // routine.
  virtual void RunBatteryRoutine(
      int low_mah,
      int high_mah,
      int* id,
      MojomCrosHealthdDiagnosticRoutineStatusEnum* status) = 0;
  virtual void RunBatterySysfsRoutine(
      int maximum_cycle_count,
      int percent_battery_wear_allowed,
      int* id,
      MojomCrosHealthdDiagnosticRoutineStatusEnum* status) = 0;
  virtual void RunUrandomRoutine(
      int length_seconds,
      int* id,
      MojomCrosHealthdDiagnosticRoutineStatusEnum* status) = 0;
  virtual void GetRoutineUpdate(
      int32_t uuid,
      MojomCrosHealthdDiagnosticRoutineCommandEnum command,
      bool include_output,
      const chromeos::cros_healthd::mojom::CrosHealthdService::
          GetRoutineUpdateCallback& callback) = 0;
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_ROUTINE_SERVICE_H_
