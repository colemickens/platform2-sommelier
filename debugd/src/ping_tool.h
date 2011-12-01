// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PING_TOOL_H
#define PING_TOOL_H

#include <map>
#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class ProcessWithId;

class PingTool {
 public:
  PingTool();
  ~PingTool();

  std::string Start(const DBus::FileDescriptor& outfd,
                    const std::string& destination,
                    const std::map<std::string, DBus::Variant>& options,
                    DBus::Error& error);
  void Stop(const std::string& handle, DBus::Error& error);
 private:
  std::map<std::string, ProcessWithId*> processes_;
  DISALLOW_COPY_AND_ASSIGN(PingTool);
};

};  // namespace debugd

#endif  // PING_TOOL_H
