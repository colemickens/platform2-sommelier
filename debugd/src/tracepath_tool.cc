// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/tracepath_tool.h"

#include "debugd/src/process_with_id.h"

namespace debugd {

namespace {

const char kTracepath[] = "/usr/sbin/tracepath";
const char kTracepath6[] = "/usr/sbin/tracepath6";

}  // namespace

std::string TracePathTool::Start(
    const dbus::FileDescriptor& outfd,
    const std::string& destination,
    const brillo::VariantDictionary& options) {
  ProcessWithId* p = CreateProcess(true);
  if (!p)
    return "";

  if (brillo::GetVariantValueOrDefault<bool>(options, "v6"))
    p->AddArg(kTracepath6);
  else
    p->AddArg(kTracepath);

  if (options.count("numeric") == 1)
    p->AddArg("-n");

  p->AddArg(destination);
  p->BindFd(outfd.value(), STDOUT_FILENO);
  p->BindFd(outfd.value(), STDERR_FILENO);
  LOG(INFO) << "tracepath: running process id: " << p->id();
  p->Start();
  return p->id();
}

}  // namespace debugd
