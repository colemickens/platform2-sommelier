// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_ROUTINE_SERVICE_IMPL_H_
#define DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_ROUTINE_SERVICE_IMPL_H_

#include <vector>

#include <base/macros.h>

#include "diagnostics/cros_healthd/cros_healthd_routine_service.h"
#include "mojo/cros_healthd.mojom.h"

namespace diagnostics {

// Production implementation of the CrosHealthdRoutineService interface.
class CrosHealthdRoutineServiceImpl final : public CrosHealthdRoutineService {
 public:
  using MojomCrosHealthdDiagnosticRoutineEnum =
      CrosHealthdRoutineService::MojomCrosHealthdDiagnosticRoutineEnum;
  using MojomCrosHealthdDiagnosticRoutineCommandEnum =
      CrosHealthdRoutineService::MojomCrosHealthdDiagnosticRoutineCommandEnum;
  using MojomCrosHealthdDiagnosticRoutineStatusEnum =
      CrosHealthdRoutineService::MojomCrosHealthdDiagnosticRoutineStatusEnum;

  CrosHealthdRoutineServiceImpl();
  ~CrosHealthdRoutineServiceImpl() override;

  // CrosHealthdRoutineService overrides:
  std::vector<MojomCrosHealthdDiagnosticRoutineEnum> GetAvailableRoutines()
      override;
  void RunBatteryRoutine(
      int low_mah,
      int high_mah,
      int* id,
      MojomCrosHealthdDiagnosticRoutineStatusEnum* status) override;
  void RunBatterySysfsRoutine(
      int maximum_cycle_count,
      int percent_battery_wear_allowed,
      int* id,
      MojomCrosHealthdDiagnosticRoutineStatusEnum* status) override;
  void RunUrandomRoutine(
      int length_seconds,
      int* id,
      MojomCrosHealthdDiagnosticRoutineStatusEnum* status) override;
  void GetRoutineUpdate(
      int32_t uuid,
      MojomCrosHealthdDiagnosticRoutineCommandEnum command,
      bool include_output,
      const chromeos::cros_healthd::mojom::CrosHealthdService::
          GetRoutineUpdateCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosHealthdRoutineServiceImpl);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_ROUTINE_SERVICE_IMPL_H_
