// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEMORY_TOOL_H
#define MEMORY_TOOL_H

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "subprocess_tool.h"

namespace debugd {

class MemtesterTool : public SubprocessTool {
 public:
  MemtesterTool();
  ~MemtesterTool();

  std::string Start(const DBus::FileDescriptor& outfd,
                    const uint32_t& memory,
                    DBus::Error& error);
};

};  // namespace debugd

#endif  // !TRACEPATH_TOOL_H
