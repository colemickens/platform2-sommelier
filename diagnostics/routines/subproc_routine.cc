// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/routines/subproc_routine.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/process/process_handle.h>

#include "diagnostics/routines/diag_process_adapter_impl.h"

namespace diagnostics {

constexpr char kSubprocRoutineProcessRunningMessage[] =
    "Test is still running.";
constexpr char kSubprocRoutineSucceededMessage[] = "Test passed.";
constexpr char kSubprocRoutineFailedMessage[] = "Test failed.";
constexpr char kSubprocRoutineFailedToLaunchProcessMessage[] =
    "Could not launch the process.";
constexpr char kSubprocRoutineProcessCrashedOrKilledMessage[] =
    "The process crashed or was killed.";
constexpr char kSubprocRoutineFailedToPauseMessage[] =
    "Failed to pause the routine.";
constexpr char kSubprocRoutineFailedToCancelMessage[] =
    "Failed to cancel the routine.";
constexpr char kSubprocRoutineFailedToStopMessage[] =
    "Failed to stop the routine.";

constexpr int kSubprocRoutineFakeProgressPercent = 0;

SubprocRoutine::SubprocRoutine()
    : SubprocRoutine(std::make_unique<DiagProcessAdapterImpl>()) {}

SubprocRoutine::SubprocRoutine(
    std::unique_ptr<DiagProcessAdapter> process_adapter)
    : status_(grpc_api::ROUTINE_STATUS_READY),
      process_adapter_(std::move(process_adapter)) {}

SubprocRoutine::~SubprocRoutine() {
  // If the routine is still running, make sure to stop it so we aren't left
  // with a zombie process.
  if (status_ == grpc_api::ROUTINE_STATUS_RUNNING)
    KillProcess(kSubprocRoutineFailedToStopMessage);
}

void SubprocRoutine::Start() {
  DCHECK_EQ(status_, grpc_api::ROUTINE_STATUS_READY);
  StartProcess();
}

void SubprocRoutine::Pause() {
  // Kill the current process, but record the completion percentage.
  DCHECK_EQ(status_, grpc_api::ROUTINE_STATUS_RUNNING);
  if (KillProcess(kSubprocRoutineFailedToPauseMessage))
    status_ = grpc_api::ROUTINE_STATUS_READY;
}

void SubprocRoutine::Resume() {
  // Call the subproc_routine program again, with a new calculated timeout based
  // on the completion percentage and current time.
  DCHECK_EQ(status_, grpc_api::ROUTINE_STATUS_READY);
  StartProcess();
}

void SubprocRoutine::Cancel() {
  // Kill the process and record the completion percentage.
  DCHECK_EQ(status_, grpc_api::ROUTINE_STATUS_RUNNING);
  if (KillProcess(kSubprocRoutineFailedToCancelMessage))
    status_ = grpc_api::ROUTINE_STATUS_CANCELLED;
}

void SubprocRoutine::PopulateStatusUpdate(
    grpc_api::GetRoutineUpdateResponse* response, bool include_output) {
  // Because the subproc_routine routine is non-interactive, we will never
  // include a user message.
  if (status_ == grpc_api::ROUTINE_STATUS_RUNNING)
    CheckProcessStatus();

  response->set_status(status_);
  response->set_progress_percent(kSubprocRoutineFakeProgressPercent);
  response->set_status_message(status_message_);
}

grpc_api::DiagnosticRoutineStatus SubprocRoutine::GetStatus() {
  return status_;
}

void SubprocRoutine::StartProcess() {
  status_ = grpc_api::ROUTINE_STATUS_RUNNING;
  base::CommandLine command_line = GetCommandLine();
  VLOG(1) << "Starting command " << base::JoinString(command_line.argv(), " ");

  if (!process_adapter_->StartProcess(command_line.argv(), &handle_)) {
    status_ = grpc_api::ROUTINE_STATUS_ERROR;
    status_message_ = kSubprocRoutineFailedToLaunchProcessMessage;
    LOG(ERROR) << kSubprocRoutineFailedToLaunchProcessMessage;
  }
}

bool SubprocRoutine::KillProcess(const std::string& failure_message) {
  // If the process has already exited, there's no need to kill it.
  CheckProcessStatus();

  if (handle_ != base::kNullProcessHandle &&
      !process_adapter_->KillProcess(handle_)) {
    // Reset the ProcessHandle, so we can't accidentally kill another process
    // which has reused our old PID.
    handle_ = base::kNullProcessHandle;
    status_ = grpc_api::ROUTINE_STATUS_ERROR;
    status_message_ = failure_message;
    LOG(ERROR) << "Failed to kill process.";
    return false;
  }

  return true;
}

void SubprocRoutine::CheckProcessStatus() {
  if (handle_ == base::kNullProcessHandle)
    return;

  switch (process_adapter_->GetStatus(handle_)) {
    case base::TERMINATION_STATUS_STILL_RUNNING:
      status_message_ = kSubprocRoutineProcessRunningMessage;
      break;
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      handle_ = base::kNullProcessHandle;
      status_ = grpc_api::ROUTINE_STATUS_PASSED;
      status_message_ = kSubprocRoutineSucceededMessage;
      break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      handle_ = base::kNullProcessHandle;
      status_ = grpc_api::ROUTINE_STATUS_FAILED;
      status_message_ = kSubprocRoutineFailedMessage;
      break;
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      handle_ = base::kNullProcessHandle;
      status_ = grpc_api::ROUTINE_STATUS_ERROR;
      status_message_ = kSubprocRoutineFailedToLaunchProcessMessage;
      break;
    default:
      handle_ = base::kNullProcessHandle;
      status_ = grpc_api::ROUTINE_STATUS_ERROR;
      status_message_ = kSubprocRoutineProcessCrashedOrKilledMessage;
      break;
  }
}

}  // namespace diagnostics
