// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSRQ_TOOL_H
#define SYSRQ_TOOL_H

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class SysrqTool {
 public:
  SysrqTool();
  ~SysrqTool();

  void LogKernelTaskStates(DBus::Error& error); // NOLINT
 private:
  DISALLOW_COPY_AND_ASSIGN(SysrqTool);
};

};  // namespace debugd

#endif  // SYSRQ_TOOL_H
