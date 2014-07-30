// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_PACKET_CAPTURE_TOOL_H_
#define DEBUGD_SRC_PACKET_CAPTURE_TOOL_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "debugd/src/subprocess_tool.h"

namespace debugd {

class ProcessWithId;

class PacketCaptureTool : public SubprocessTool {
 public:
  PacketCaptureTool();
  virtual ~PacketCaptureTool();

  std::string Start(
      const DBus::FileDescriptor& status_fd,
      const DBus::FileDescriptor& output_fd,
      const std::map<std::string, DBus::Variant>& options,
      DBus::Error* error);

 private:
  static bool AddValidatedStringOption(
      const std::map<std::string, DBus::Variant>& options,
      const std::string& dbus_option,
      const std::string& command_line_option,
      ProcessWithId* p);

  DISALLOW_COPY_AND_ASSIGN(PacketCaptureTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_PACKET_CAPTURE_TOOL_H_
