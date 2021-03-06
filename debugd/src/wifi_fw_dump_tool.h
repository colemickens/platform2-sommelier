// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_WIFI_FW_DUMP_TOOL_H_
#define DEBUGD_SRC_WIFI_FW_DUMP_TOOL_H_

#include <base/macros.h>

#include <string>

namespace debugd {

class WifiFWDumpTool {
 public:
  WifiFWDumpTool() = default;
  ~WifiFWDumpTool() = default;
  // Trigger WiFi firmware dumper.
  std::string WifiFWDump();

 private:
  DISALLOW_COPY_AND_ASSIGN(WifiFWDumpTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_WIFI_FW_DUMP_TOOL_H_
