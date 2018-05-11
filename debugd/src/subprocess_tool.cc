// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/subprocess_tool.h"

#include <memory>
#include <utility>

#include <base/stl_util.h>

#include "debugd/src/error_utils.h"

namespace debugd {

namespace {

const char kErrorNoSuchProcess[] = "org.chromium.debugd.error.NoSuchProcess";

}  // namespace

ProcessWithId* SubprocessTool::CreateProcess(bool sandboxed,
                                             bool access_root_mount_ns) {
  auto process = std::make_unique<ProcessWithId>();
  if (!sandboxed)
    process->DisableSandbox();

  if (access_root_mount_ns)
    process->AllowAccessRootMountNamespace();

  if (!process->Init())
    return nullptr;

  ProcessWithId* process_ptr = process.get();
  if (RecordProcess(std::move(process)))
    return process_ptr;

  return nullptr;
}

bool SubprocessTool::RecordProcess(std::unique_ptr<ProcessWithId> process) {
  if (base::ContainsKey(processes_, process->id()))
    return false;

  processes_[process->id()] = std::move(process);
  return true;
}

bool SubprocessTool::Stop(const std::string& handle, brillo::ErrorPtr* error) {
  if (processes_.count(handle) != 1) {
    DEBUGD_ADD_ERROR(error, kErrorNoSuchProcess, handle.c_str());
    return false;
  }
  ProcessWithId* process_ptr = processes_[handle].get();
  process_ptr->KillProcessGroup();
  processes_.erase(handle);
  return true;
}

}  // namespace debugd
