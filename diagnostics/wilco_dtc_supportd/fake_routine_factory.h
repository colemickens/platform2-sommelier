// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_FAKE_ROUTINE_FACTORY_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_FAKE_ROUTINE_FACTORY_H_

#include <cstdint>
#include <memory>
#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/routines/diag_routine.h"
#include "diagnostics/wilco_dtc_supportd/routine_factory.h"
#include "mojo/cros_healthd_diagnostics.mojom.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Implementation of RoutineFactory that should only be used for
// testing.
class FakeRoutineFactory final : public RoutineFactory {
 public:
  FakeRoutineFactory();
  ~FakeRoutineFactory() override;

  // Makes the next routine returned by CreateRoutine report an interactive
  // status with the specified user message, progress_percent and output. Any
  // future calls to this function or SetInteractiveStatus will override the
  // settings from a previous call to SetInteractiveStatus or
  // SetNoninteractiveStatus.
  void SetInteractiveStatus(
      chromeos::cros_healthd::mojom::DiagnosticRoutineUserMessageEnum
          user_message,
      uint32_t progress_percent,
      const std::string& output);

  // Makes the next routine returned by CreateRoutine report a noninteractive
  // status with the specified status, status_message, progress_percent and
  // output. Any future calls to this function or SetInteractiveStatus will
  // override the settings from a previous call to SetInteractiveStatus or
  // SetNonInteractiveStatus.
  void SetNonInteractiveStatus(
      chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum status,
      const std::string& status_message,
      uint32_t progress_percent,
      const std::string& output);

  // RoutineFactory overrides:
  std::unique_ptr<DiagnosticRoutine> CreateRoutine(
      const grpc_api::RunRoutineRequest& request) override;

 private:
  // The routine that will be returned by CreateRoutine.
  std::unique_ptr<DiagnosticRoutine> next_routine_;

  DISALLOW_COPY_AND_ASSIGN(FakeRoutineFactory);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_FAKE_ROUTINE_FACTORY_H_
