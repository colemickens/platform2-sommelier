// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUG_LOGS_TOOL_H
#define DEBUG_LOGS_TOOL_H

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class DebugLogsTool {
 public:
  DebugLogsTool();
  ~DebugLogsTool();

  void GetDebugLogs(const DBus::FileDescriptor& fd, DBus::Error& error);
 private:
  DISALLOW_COPY_AND_ASSIGN(DebugLogsTool);
};

};  // namespace debugd

#endif  // DEBUG_LOGS_TOOL_H
