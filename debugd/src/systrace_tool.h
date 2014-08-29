// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_SYSTRACE_TOOL_H_
#define DEBUGD_SRC_SYSTRACE_TOOL_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "debugd/src/subprocess_tool.h"

namespace debugd {

class SystraceTool {
 public:
  SystraceTool() = default;
  ~SystraceTool() = default;

  std::string Start(const std::string& categories, DBus::Error* error);
  void Stop(const DBus::FileDescriptor& outfd, DBus::Error* error);
  std::string Status(DBus::Error* error);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_SYSTRACE_TOOL_H_
