// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PING_TOOL_H_
#define PING_TOOL_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "subprocess_tool.h"

namespace debugd {

class PingTool : public SubprocessTool {
 public:
  PingTool();
  virtual ~PingTool();

  std::string Start(const DBus::FileDescriptor& outfd,
                    const std::string& destination,
                    const std::map<std::string, DBus::Variant>& options,
                    DBus::Error* error);
};

}  // namespace debugd

#endif  // PING_TOOL_H_
