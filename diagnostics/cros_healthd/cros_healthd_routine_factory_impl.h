// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_ROUTINE_FACTORY_IMPL_H_
#define DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_ROUTINE_FACTORY_IMPL_H_

#include <cstdint>
#include <memory>

#include <base/macros.h>

#include "diagnostics/cros_healthd/cros_healthd_routine_factory.h"

namespace diagnostics {

// Production implementation of the CrosHealthdRoutineFactory interface.
class CrosHealthdRoutineFactoryImpl final : public CrosHealthdRoutineFactory {
 public:
  CrosHealthdRoutineFactoryImpl();
  ~CrosHealthdRoutineFactoryImpl() override;

  // CrosHealthdRoutineFactory overrides:
  std::unique_ptr<DiagnosticRoutine> MakeUrandomRoutine(
      uint32_t length_seconds) override;
  std::unique_ptr<DiagnosticRoutine> MakeBatteryCapacityRoutine(
      uint32_t low_mah, uint32_t high_mah) override;
  std::unique_ptr<DiagnosticRoutine> MakeBatteryHealthRoutine(
      uint32_t maximum_cycle_count,
      uint32_t percent_battery_wear_allowed) override;
  std::unique_ptr<DiagnosticRoutine> MakeSmartctlCheckRoutine() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosHealthdRoutineFactoryImpl);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_ROUTINE_FACTORY_IMPL_H_
