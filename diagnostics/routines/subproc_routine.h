// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_ROUTINES_SUBPROC_ROUTINE_H_
#define DIAGNOSTICS_ROUTINES_SUBPROC_ROUTINE_H_

#include <cstdint>
#include <memory>
#include <string>

#include <base/command_line.h>
#include <base/macros.h>
#include <base/process/process.h>
#include <base/time/default_tick_clock.h>

#include "diagnostics/routines/diag_process_adapter.h"
#include "diagnostics/routines/diag_routine.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Output messages for the routine when in various states.
extern const char kSubprocRoutineReady[];
extern const char kSubprocRoutineProcessRunningMessage[];
extern const char kSubprocRoutineSucceededMessage[];
extern const char kSubprocRoutineFailedMessage[];
extern const char kSubprocRoutineFailedToLaunchProcessMessage[];
extern const char kSubprocRoutineProcessCrashedOrKilledMessage[];
extern const char kSubprocRoutineFailedToStopMessage[];
extern const char kSubprocRoutineInvalidParametersMessage[];

// We don't always know when a SubprocRoutine should finish. Sometimes we have
// to fake our prediction of percent complete.
extern const int kSubprocRoutineFakeProgressPercentUnknown;
extern const int kSubprocRoutineFakeProgressPercentKilling;

// Progress percentage when the routine has been running for equal to
// or longer than the time specified in the SubprocRoutineRoutineParameters, but
// has not stopped yet.
extern const int64_t kSubprocRoutineLongRunningProgress;

// The SubprocRoutine takes a command line to run. It is non-interactive, and
// this does not fully support Pause and Resume. Pause will simply kill the
// process. The exit code of the process is used to determine success or failure
// of the test. So, the "check" portion of the Routine must live inside the
// sub-process.
class SubprocRoutine final : public DiagnosticRoutine {
 public:
  // The state of the SubprocRoutine is modeled in the SubprocStatus enum.
  enum SubprocStatus {
    kSubprocStatusCanceled,
    kSubprocStatusCompleteFailure,
    kSubprocStatusCompleteSuccess,
    kSubprocStatusError,
    kSubprocStatusKilling,
    kSubprocStatusLaunchFailed,
    kSubprocStatusReady,
    kSubprocStatusRunning,
  };

  SubprocRoutine(const base::CommandLine& command_line,
                 int predicted_duration_in_seconds);
  SubprocRoutine(std::unique_ptr<DiagProcessAdapter> process_adapter,
                 std::unique_ptr<base::TickClock> tick_clock,
                 const base::CommandLine& command_line,
                 int predicted_duration_in_seconds);

  // DiagnosticRoutine overrides:
  ~SubprocRoutine() override;
  void Start() override;
  void Resume() override;
  void Cancel() override;
  void PopulateStatusUpdate(grpc_api::GetRoutineUpdateResponse* response,
                            bool include_output) override;
  grpc_api::DiagnosticRoutineStatus GetStatus() override;

 private:
  // Functions to manipulate the child process.
  void StartProcess();
  void KillProcess(bool from_dtor);
  // Handle state transitions due to process state within this object.
  void CheckProcessStatus();
  void CheckActiveProcessStatus();
  int CalculateProgressPercent() const;

  SubprocStatus subproc_status_;
  std::unique_ptr<DiagProcessAdapter> process_adapter_;
  std::unique_ptr<base::TickClock> tick_clock_;
  base::CommandLine command_line_;
  int predicted_duration_in_seconds_ = 0;
  base::ProcessHandle handle_ = base::kNullProcessHandle;
  base::TimeTicks start_ticks_;

  DISALLOW_COPY_AND_ASSIGN(SubprocRoutine);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_ROUTINES_SUBPROC_ROUTINE_H_
