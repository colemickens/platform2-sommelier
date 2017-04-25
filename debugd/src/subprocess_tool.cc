// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/subprocess_tool.h"

#include <utility>

#include <base/memory/ptr_util.h>

namespace debugd {

namespace {

const char kErrorNoSuchProcess[] = "org.chromium.debugd.error.NoSuchProcess";

}  // namespace

ProcessWithId* SubprocessTool::CreateProcess(bool sandboxed) {
  return CreateProcess(sandboxed, false);
}

ProcessWithId* SubprocessTool::CreateProcess(bool sandboxed,
                                             bool access_root_mount_ns) {
  auto process = base::MakeUnique<ProcessWithId>();
  if (!sandboxed)
    process->DisableSandbox();

  if (access_root_mount_ns)
    process->AllowAccessRootMountNamespace();

  if (!process->Init() || processes_.count(process->id()) == 1)
    return nullptr;

  ProcessWithId* process_ptr = process.get();
  processes_[process->id()] = std::move(process);
  return process_ptr;
}

bool SubprocessTool::Stop(const std::string& handle, DBus::Error* error) {
  if (processes_.count(handle) != 1) {
    error->set(kErrorNoSuchProcess, handle.c_str());
    return false;
  }
  ProcessWithId* process_ptr = processes_[handle].get();
  process_ptr->KillProcessGroup();
  processes_.erase(handle);
  return true;
}

}  // namespace debugd
