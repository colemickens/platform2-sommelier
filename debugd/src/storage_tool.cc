// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/storage_tool.h"

#include "debugd/src/process_with_id.h"
#include "debugd/src/process_with_output.h"

using base::StringPrintf;

namespace debugd {

const char* kSmartctl = "/usr/sbin/smartctl";
const char* kBadblocks = "/sbin/badblocks";
const char* kDevice = "/dev/sda";

std::string StorageTool::Smartctl(const std::string& option,
                                  DBus::Error* error) {
  std::string path;
  if (!SandboxedProcess::GetHelperPath("storage", &path))
    return "<path too long>";

  ProcessWithOutput process;
  // Disabling sandboxing since smartctl requires higher privileges.
  process.DisableSandbox();
  if (!process.Init())
    return "<process init failed>";

  process.AddArg(kSmartctl);

  if (option == "abort_test")
    process.AddArg("-X");
  if (option == "attributes")
    process.AddArg("-A");
  if (option == "capabilities")
    process.AddArg("-c");
  if (option == "error")
    process.AddStringOption("-l", "error");
  if (option == "health")
    process.AddArg("-H");
  if (option == "selftest")
    process.AddStringOption("-l", "selftest");
  if (option == "short_test")
    process.AddStringOption("-t", "short");

  process.AddArg(kDevice);
  process.Run();
  std::string output;
  process.GetOutput(&output);
  return output;
}

std::string StorageTool::Start(const DBus::FileDescriptor& outfd,
                               DBus::Error* error) {
  ProcessWithId* p = CreateProcess(false);
  if (!p)
    return "";

  p->AddArg(kBadblocks);
  p->AddArg("-sv");
  p->AddArg(kDevice);
  p->BindFd(outfd.get(), STDOUT_FILENO);
  p->BindFd(outfd.get(), STDERR_FILENO);
  LOG(INFO) << "badblocks: running process id: " << p->id();
  p->Start();
  return p->id();
}

}  // namespace debugd
