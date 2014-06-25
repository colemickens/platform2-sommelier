// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRACEPATH_TOOL_H_
#define TRACEPATH_TOOL_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "subprocess_tool.h"

namespace debugd {

class TracePathTool : public SubprocessTool {
 public:
  TracePathTool();
  virtual ~TracePathTool();

  std::string Start(const DBus::FileDescriptor& outfd,
                    const std::string& destination,
                    const std::map<std::string, DBus::Variant>& options,
                    DBus::Error* error);
};

}  // namespace debugd

#endif  // TRACEPATH_TOOL_H_
