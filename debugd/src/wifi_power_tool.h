// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_WIFI_POWER_TOOL_H_
#define DEBUGD_SRC_WIFI_POWER_TOOL_H_

#include <string>

#include <base/macros.h>

namespace debugd {

// Gets and sets WiFi power save mode.
class WifiPowerTool {
 public:
  WifiPowerTool() = default;
  ~WifiPowerTool() = default;

  // Sets the power save mode and returns the new mode, or an error.
  std::string SetWifiPowerSave(bool enable) const;

  // Returns the current power save mode.
  std::string GetWifiPowerSave() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(WifiPowerTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_WIFI_POWER_TOOL_H_
