// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_MODEM_STATUS_TOOL_H_
#define DEBUGD_SRC_MODEM_STATUS_TOOL_H_

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class ModemStatusTool {
 public:
  ModemStatusTool() = default;
  ~ModemStatusTool() = default;
  std::string GetModemStatus(DBus::Error* error);
  std::string RunModemCommand(const std::string& command);

 private:
  friend class ModemStatusToolTest;

  std::string SendATCommand(const std::string& command);
  static std::string CollapseNewLines(const std::string& input);

  DISALLOW_COPY_AND_ASSIGN(ModemStatusTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_MODEM_STATUS_TOOL_H_
