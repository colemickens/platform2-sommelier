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

#include "diagnostics/routines/diag_process_adapter.h"
#include "diagnostics/routines/diag_routine.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Output messages for the routine when in various states.
extern const char kSubprocRoutineProcessRunningMessage[];
extern const char kSubprocRoutineSucceededMessage[];
extern const char kSubprocRoutineFailedMessage[];
extern const char kSubprocRoutineFailedToLaunchProcessMessage[];
extern const char kSubprocRoutineProcessCrashedOrKilledMessage[];
extern const char kSubprocRoutineFailedToPauseMessage[];
extern const char kSubprocRoutineFailedToCancelMessage[];
extern const char kSubprocRoutineFailedToStopMessage[];
extern const char kSubprocRoutineInvalidParametersMessage[];

// SubprocRoutines do not generally know their progress. Use a fake value.
extern const int kSubprocRoutineFakeProgressPercent;

// Progress percentage when the routine has been running for equal to
// or longer than the time specified in the SubprocRoutineRoutineParameters, but
// has not stopped yet.
extern const int64_t kSubprocRoutineLongRunningProgress;

// The SubprocRoutine takes a command line to run. It is non-interactive, and
// this does not fully support Pause and Resume. Pause will simply kill the
// process. The exit code of the process is used to determine success or failure
// of the test. So, the "check" portion of the Routine must live inside the
// sub-process.
class SubprocRoutine : public DiagnosticRoutine {
 public:
  SubprocRoutine();
  explicit SubprocRoutine(std::unique_ptr<DiagProcessAdapter> process_adapter);

  // DiagnosticRoutine overrides:
  ~SubprocRoutine() override;
  void Start() override;
  void Pause() override;
  void Resume() override;
  void Cancel() override;
  void PopulateStatusUpdate(grpc_api::GetRoutineUpdateResponse* response,
                            bool include_output) override;
  grpc_api::DiagnosticRoutineStatus GetStatus() override;

 protected:
  // Overrides for subclasses.
  virtual base::CommandLine GetCommandLine() const = 0;

 private:
  // Functions to manipulate the child process.
  void StartProcess();
  bool KillProcess(const std::string& failure_message);
  void CheckProcessStatus();

  grpc_api::DiagnosticRoutineStatus status_;
  std::string status_message_;
  std::unique_ptr<DiagProcessAdapter> process_adapter_;
  base::ProcessHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(SubprocRoutine);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_ROUTINES_SUBPROC_ROUTINE_H_
