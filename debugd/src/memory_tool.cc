// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/memory_tool.h"

#include "debugd/src/process_with_id.h"

namespace debugd {

namespace {

const char kMemtesterpath[] = "/usr/sbin/memtester";

}  // namespace

std::string MemtesterTool::Start(const base::ScopedFD& outfd,
                                 const uint32_t& memory) {
  ProcessWithId* p =
      CreateProcess(false /* sandboxed */, false /* access_root_mount_ns */);
  if (!p)
    return "";

  p->AddArg(kMemtesterpath);
  p->AddArg(base::StringPrintf("%u", memory));
  p->AddArg("1");
  p->BindFd(outfd.get(), STDOUT_FILENO);
  p->BindFd(outfd.get(), STDERR_FILENO);
  LOG(INFO) << "memtester: running process id: " << p->id();
  p->Start();
  return p->id();
}

}  // namespace debugd
