// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_DEBUG_MODE_TOOL_H_
#define DEBUGD_SRC_DEBUG_MODE_TOOL_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <dbus/bus.h>

namespace debugd {

class DebugModeTool {
 public:
  explicit DebugModeTool(scoped_refptr<dbus::Bus> bus);
  virtual ~DebugModeTool() = default;

  virtual void SetDebugMode(const std::string& subsystem);

 private:
  void SetModemManagerLogging(const std::string& level);

  scoped_refptr<dbus::Bus> bus_;

  DISALLOW_COPY_AND_ASSIGN(DebugModeTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_DEBUG_MODE_TOOL_H_
