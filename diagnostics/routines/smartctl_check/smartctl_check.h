// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_ROUTINES_SMARTCTL_CHECK_SMARTCTL_CHECK_H_
#define DIAGNOSTICS_ROUTINES_SMARTCTL_CHECK_SMARTCTL_CHECK_H_

#include <memory>

#include <base/command_line.h>

#include "diagnostics/routines/subproc_routine.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

constexpr char kSmartctlCheckExecutableInstallLocation[] =
    "/usr/libexec/diagnostics/smartctl-check";

// The smartctl_check routine takes two parameters, the device path to test, and
// the minimum kilobytes per second that it must acheive to pass. The
// implementation is handled in the smartctl-check program.
class SmartctlCheckRoutine final : public SubprocRoutine {
 public:
  explicit SmartctlCheckRoutine(
      const grpc_api::SmartctlCheckRoutineParameters& parameters);
  SmartctlCheckRoutine(
      const grpc_api::SmartctlCheckRoutineParameters& parameters,
      std::unique_ptr<DiagProcessAdapter> process_adapter);

  // DiagnosticRoutine overrides:
  ~SmartctlCheckRoutine() override;

 private:
  base::CommandLine GetCommandLine() const override;

  grpc_api::SmartctlCheckRoutineParameters parameters_;

  DISALLOW_COPY_AND_ASSIGN(SmartctlCheckRoutine);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_ROUTINES_SMARTCTL_CHECK_SMARTCTL_CHECK_H_
