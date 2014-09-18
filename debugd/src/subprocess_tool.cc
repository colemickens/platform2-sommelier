// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/subprocess_tool.h"

#include <signal.h>

#include <dbus-c++/dbus.h>

#include "debugd/src/process_with_id.h"

namespace debugd {

namespace {

const char kErrorNoSuchProcess[] = "org.chromium.debugd.error.NoSuchProcess";

}  // namespace

ProcessWithId* SubprocessTool::CreateProcess(bool sandboxed) {
  return CreateProcess(sandboxed, false);
}

ProcessWithId* SubprocessTool::CreateProcess(bool sandboxed,
                                             bool access_root_mount_ns) {
  ProcessWithId* p = new ProcessWithId();
  if (!sandboxed)
    p->DisableSandbox();
  if (access_root_mount_ns)
    p->AllowAccessRootMountNamespace();
  if (!p->Init() || processes_.count(p->id()) == 1)
    return NULL;
  processes_[p->id()] = p;
  return p;
}

void SubprocessTool::Stop(const std::string& handle, DBus::Error* error) {
  if (processes_.count(handle) != 1) {
    error->set(kErrorNoSuchProcess, handle.c_str());
    return;
  }
  ProcessWithId* p = processes_[handle];
  p->Kill(SIGKILL, 0);  // no timeout
  p->Wait();
  processes_.erase(handle);
  delete p;
}

}  // namespace debugd
