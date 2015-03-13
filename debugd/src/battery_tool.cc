// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/battery_tool.h"

#include "debugd/src/process_with_id.h"
#include "debugd/src/process_with_output.h"

using base::StringPrintf;

namespace {

const char kBatteryFirmware[] = "/usr/sbin/ec_sb_firmware_update";
const char kEcTool[] = "/usr/sbin/ectool";

}  // namespace


namespace debugd {

std::string BatteryTool::BatteryFirmware(const std::string& option,
                                         DBus::Error* error) {
  std::string output;
  ProcessWithOutput process;
  // Disabling sandboxing since battery requires higher privileges.
  process.DisableSandbox();
  if (!process.Init())
    return "<process init failed>";

  if (option == "info") {
    process.AddArg(kEcTool);
    process.AddArg("battery");
  } else if (option == "update") {
    process.AddArg(kBatteryFirmware);
    process.AddArg("update");
  } else if (option == "check") {
    process.AddArg(kBatteryFirmware);
    process.AddArg("check");
  } else {
    return "<process invalid option>";
  }

  process.Run();
  process.GetOutput(&output);
  return output;
}

}  // namespace debugd
