// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SUBPROCESS_TOOL_H
#define SUBPROCESS_TOOL_H

#include <map>
#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class ProcessWithId;

class SubprocessTool {
 public:
  SubprocessTool();
  virtual ~SubprocessTool();

  virtual ProcessWithId* CreateProcess();
  virtual void Stop(const std::string& handle, DBus::Error& error);
 private:
  std::map<std::string, ProcessWithId*> processes_;
  DISALLOW_COPY_AND_ASSIGN(SubprocessTool);
};

};  // namespace debugd

#endif  // SUBPROCESS_TOOL_H
