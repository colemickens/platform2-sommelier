// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "subprocess_tool.h"

#include <signal.h>

#include <dbus-c++/dbus.h>

#include "process_with_id.h"

namespace debugd {

namespace {

const char kErrorNoSuchProcess[] = "org.chromium.debugd.error.NoSuchProcess";

}  // namespace

SubprocessTool::SubprocessTool() { }
SubprocessTool::~SubprocessTool() { }

ProcessWithId* SubprocessTool::CreateProcess(bool sandbox) {
  ProcessWithId* p = new ProcessWithId();
  if (!sandbox)
    p->DisableSandbox();
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
