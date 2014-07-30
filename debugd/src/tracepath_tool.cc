// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/tracepath_tool.h"

#include "debugd/src/process_with_id.h"

namespace debugd {

const char kTracepath[] = "/usr/sbin/tracepath";

TracePathTool::TracePathTool() { }
TracePathTool::~TracePathTool() { }

std::string TracePathTool::Start(
    const DBus::FileDescriptor& outfd,
    const std::string& destination,
    const std::map<std::string, DBus::Variant>& options,
    DBus::Error* error) {
  ProcessWithId* p = CreateProcess(true);
  if (!p)
    return "";
  p->AddArg(kTracepath);
  if (options.count("numeric") == 1) {
    p->AddArg("-n");
  }
  p->AddArg(destination);
  p->BindFd(outfd.get(), STDOUT_FILENO);
  p->BindFd(outfd.get(), STDERR_FILENO);
  LOG(INFO) << "tracepath: running process id: " << p->id();
  p->Start();
  return p->id();
}

}  // namespace debugd
