// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_MODEM_STATUS_TOOL_H_
#define DEBUGD_SRC_MODEM_STATUS_TOOL_H_

#include <string>

#include <base/macros.h>

namespace debugd {

class ModemStatusTool {
 public:
  ModemStatusTool() = default;
  ~ModemStatusTool() = default;
  std::string GetModemStatus();

 private:
  DISALLOW_COPY_AND_ASSIGN(ModemStatusTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_MODEM_STATUS_TOOL_H_
