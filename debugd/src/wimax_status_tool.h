// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_WIMAX_STATUS_TOOL_H_
#define DEBUGD_SRC_WIMAX_STATUS_TOOL_H_

#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class WiMaxStatusTool {
 public:
  WiMaxStatusTool() = default;
  ~WiMaxStatusTool() = default;

  std::string GetWiMaxStatus(DBus::Error* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(WiMaxStatusTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_WIMAX_STATUS_TOOL_H_
