// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUG_MODE_TOOL_H
#define DEBUG_MODE_TOOL_H

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class DebugModeTool {
 public:
  explicit DebugModeTool(DBus::Connection* connection);
  virtual ~DebugModeTool();

  virtual void SetDebugMode(const std::string& subsystem, DBus::Error* error);
 private:
  DBus::Connection* connection_;
  DISALLOW_COPY_AND_ASSIGN(DebugModeTool);
};

};  // namespace debugd

#endif  // DEBUG_MODE_TOOL_H
