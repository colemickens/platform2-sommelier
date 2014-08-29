// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_SYSRQ_TOOL_H_
#define DEBUGD_SRC_SYSRQ_TOOL_H_

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class SysrqTool {
 public:
  SysrqTool() = default;
  ~SysrqTool() = default;

  void LogKernelTaskStates(DBus::Error* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(SysrqTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_SYSRQ_TOOL_H_
