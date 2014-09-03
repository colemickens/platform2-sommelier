// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_DEBUG_LOGS_TOOL_H_
#define DEBUGD_SRC_DEBUG_LOGS_TOOL_H_

#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class DebugLogsTool {
 public:
  DebugLogsTool() = default;
  ~DebugLogsTool() = default;

  void GetDebugLogs(bool is_compressed,
                    const DBus::FileDescriptor& fd,
                    DBus::Error* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(DebugLogsTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_DEBUG_LOGS_TOOL_H_
