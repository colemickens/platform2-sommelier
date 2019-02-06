// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/routines/diag_process_adapter_impl.h"

#include <stdlib.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/process/kill.h>
#include <base/process/launch.h>
#include <base/process/process.h>
#include <base/process/process_handle.h>

namespace diagnostics {

DiagProcessAdapterImpl::DiagProcessAdapterImpl() = default;

DiagProcessAdapterImpl::~DiagProcessAdapterImpl() = default;

base::TerminationStatus DiagProcessAdapterImpl::GetStatus(
    const base::ProcessHandle& handle) const {
  return base::GetTerminationStatus(handle, nullptr);
}

bool DiagProcessAdapterImpl::StartProcess(const std::vector<std::string>& args,
                                          base::ProcessHandle* handle) {
  exe_path_ = args[0];
  base::Process process =
      base::LaunchProcess(base::CommandLine(args), base::LaunchOptions());
  if (process.IsValid()) {
    *handle = process.Handle();
    return true;
  }
  return false;
}

bool DiagProcessAdapterImpl::KillProcess(const base::ProcessHandle& handle) {
  DCHECK_NE(handle, base::kNullProcessHandle);
  return base::Process(handle).Terminate(EXIT_FAILURE, false);
}

}  // namespace diagnostics
