// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_WIFI_DEBUG_TOOL_H_
#define DEBUGD_SRC_WIFI_DEBUG_TOOL_H_

#include <base/files/file_path.h>
#include <base/macros.h>
#include <dbus-c++/dbus.h>

namespace debugd {

enum WifiDebugFlag {
  WIFI_DEBUG_ENABLED = 1 << 0,
};

// Enable/disable extra Wifi driver debugging info (e.g., in dmesg).
class WifiDebugTool {
 public:
  WifiDebugTool() = default;
  ~WifiDebugTool() = default;

  bool SetEnabled(WifiDebugFlag flags, DBus::Error* error);

 private:
  bool WriteSysfsFlags(const char* str, base::FilePath path,
                       DBus::Error* error);
  DISALLOW_COPY_AND_ASSIGN(WifiDebugTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_WIFI_DEBUG_TOOL_H_
