// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODEM_STATUS_TOOL_H
#define MODEM_STATUS_TOOL_H

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class ModemStatusTool {
 public:
  ModemStatusTool();
  ~ModemStatusTool();

  std::string GetModemStatus(DBus::Error& error); // NOLINT

  std::string RunModemCommand(const std::string& command); // NOLINT

 private:
  friend class ModemStatusToolTest;

  std::string SendATCommand(const std::string& command); // NOLINT

  static std::string CollapseNewLines(const std::string& input);  // NOLINT

  DISALLOW_COPY_AND_ASSIGN(ModemStatusTool);
};

};  // namespace debugd

#endif  // MODEM_STATUS_TOOL_H
