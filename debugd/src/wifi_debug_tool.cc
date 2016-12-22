// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/wifi_debug_tool.h"

#include <base/files/file_util.h>

#include <string.h>

namespace debugd {

namespace {

const char kErrorWifiDebug[] = "org.chromium.debugd.error.WifiDebug";

const char kMwifiexDebugMask[] = "/sys/kernel/debug/mwifiex/mlan0/debug_mask";
// Enable extra debugging: MSG | FATAL | ERROR | CMD | EVENT.
const char kMwifiexEnable[] = "0x37";
// Default debugging level: MSG | FATAL | ERROR.
const char kMwifiexDisable[] = "0x7";

}  // namespace

// Return 1 if success; 0 if not supported; negative on error.
int WifiDebugTool::MwifiexDebug(bool enable, DBus::Error* error) {
  base::FilePath path(kMwifiexDebugMask);

  if (!base::PathExists(path))
    return 0;

  const char *str = enable ? kMwifiexEnable : kMwifiexDisable;
  int len = strlen(str);
  if (base::WriteFile(path, str, len) != len) {
    error->set(kErrorWifiDebug, "write");
    return -1;
  }
  return 1;
}

// Supports only mwifiex for now.
bool WifiDebugTool::SetEnabled(WifiDebugFlag flags, DBus::Error* error) {
  if (flags & ~WIFI_DEBUG_ENABLED) {
    error->set(kErrorWifiDebug, "unsupported flags");
    return false;
  }

  return MwifiexDebug(flags == WIFI_DEBUG_ENABLED, error) == 1;
}

}  // namespace debugd
