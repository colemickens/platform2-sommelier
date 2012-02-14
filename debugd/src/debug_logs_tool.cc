// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug_logs_tool.h"

#include <base/file_util.h>
#include <chromeos/process.h>

namespace debugd {

const char* const kTar = "/bin/tar";
const char* const kSystemLogs = "/var/log";

DebugLogsTool::DebugLogsTool() { }
DebugLogsTool::~DebugLogsTool() { }

void DebugLogsTool::GetDebugLogs(const DBus::FileDescriptor& fd,
                                 DBus::Error& error) {
  chromeos::Process* p = new chromeos::ProcessImpl();
  p->AddArg(kTar);
  p->AddArg("-c");
  p->AddArg("-z");
  p->AddArg(kSystemLogs);
  p->BindFd(fd.get(), STDOUT_FILENO);
  p->Run();
}

};  // namespace debugd
