// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/sysrq_tool.h"

#include <fcntl.h>
#include <unistd.h>

namespace debugd {

const char kErrorSysrq[] = "org.chromium.debugd.error.sysrq";

SysrqTool::SysrqTool() { }

SysrqTool::~SysrqTool() { }

void SysrqTool::LogKernelTaskStates(DBus::Error* error) {
  int sysrq_trigger = open("/proc/sysrq-trigger", O_WRONLY | O_CLOEXEC);
  if (sysrq_trigger < 0) {
    error->set(kErrorSysrq, "open");
    return;
  }
  ssize_t written = write(sysrq_trigger, "t", 1);
  close(sysrq_trigger);
  if (written < 1) {
    error->set(kErrorSysrq, "write");
    return;
  }
}

}  // namespace debugd
