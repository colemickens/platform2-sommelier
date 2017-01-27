// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/wifi_debug_tool.h"

#include <base/files/file_util.h>

#include <string.h>

namespace debugd {

namespace {

const char kErrorWifiDebug[] = "org.chromium.debugd.error.WifiDebug";

// Marvell wifi
const char kMwifiexDebugMask[] = "/sys/kernel/debug/mwifiex/mlan0/debug_mask";
// Enable extra debugging: MSG | FATAL | ERROR | CMD | EVENT.
const char kMwifiexEnable[] = "0x37";
// Default debugging level: MSG | FATAL | ERROR.
const char kMwifiexDisable[] = "0x7";

// Intel wifi
const char kIwlwifiDebugFlag[] = "/sys/module/iwlwifi/parameters/debug";
// Full debugging: see below file for details on each bit:
// drivers/net/wireless-$(WIFIVERSION)/iwl7000/iwlwifi/iwl-debug.h
const char kIwlwifiEnable[] = "0xFFFFFFFF";
// Default debugging: none
const char kIwlwifiDisable[] = "0x0";

}  // namespace

bool WifiDebugTool::WriteSysfsFlags(const char* str, base::FilePath path,
                                    DBus::Error* error) {
  int len = strlen(str);
  if (base::WriteFile(path, str, len) != len) {
    error->set(kErrorWifiDebug, "write");
    return false;
  }
  return true;
}

bool WifiDebugTool::SetEnabled(WifiDebugFlag flags, DBus::Error* error) {
  if (flags & ~WIFI_DEBUG_ENABLED) {
    error->set(kErrorWifiDebug, "unsupported flags");
    return false;
  }

  base::FilePath iwlwifi_path(kIwlwifiDebugFlag);
  base::FilePath mwifiex_path(kMwifiexDebugMask);
  const char* str = nullptr;

  if (base::PathExists(iwlwifi_path)) {
    str = (flags == WIFI_DEBUG_ENABLED) ? kIwlwifiEnable : kIwlwifiDisable;
    return WriteSysfsFlags(str, iwlwifi_path, error);
  }
  if (base::PathExists(mwifiex_path)) {
    str = (flags == WIFI_DEBUG_ENABLED) ? kMwifiexEnable : kMwifiexDisable;
    return WriteSysfsFlags(str, mwifiex_path, error);
  }
  error->set(kErrorWifiDebug, "unsupported driver");
  return false;
}

}  // namespace debugd
