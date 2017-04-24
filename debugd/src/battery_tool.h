// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_BATTERY_TOOL_H_
#define DEBUGD_SRC_BATTERY_TOOL_H_

#include <string>

#include <base/macros.h>

#include "debugd/src/subprocess_tool.h"

namespace debugd {

class BatteryTool : public SubprocessTool {
 public:
  BatteryTool() = default;
  ~BatteryTool() override = default;

  std::string BatteryFirmware(const std::string& option);

 private:
  DISALLOW_COPY_AND_ASSIGN(BatteryTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_BATTERY_TOOL_H_
