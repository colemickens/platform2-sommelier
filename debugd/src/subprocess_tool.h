// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_SUBPROCESS_TOOL_H_
#define DEBUGD_SRC_SUBPROCESS_TOOL_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class ProcessWithId;

class SubprocessTool {
 public:
  SubprocessTool() = default;
  virtual ~SubprocessTool() = default;

  virtual ProcessWithId* CreateProcess(bool sandboxed);
  virtual ProcessWithId* CreateProcess(bool sandboxed,
                                       bool allow_root_mount_ns);

  virtual void Stop(const std::string& handle, DBus::Error* error);

 private:
  std::map<std::string, ProcessWithId*> processes_;

  DISALLOW_COPY_AND_ASSIGN(SubprocessTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_SUBPROCESS_TOOL_H_
