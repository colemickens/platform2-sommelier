// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_ROUTE_TOOL_H_
#define DEBUGD_SRC_ROUTE_TOOL_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class RouteTool {
 public:
  RouteTool() = default;
  ~RouteTool() = default;

  std::vector<std::string> GetRoutes(
      const std::map<std::string, DBus::Variant>& options, DBus::Error* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(RouteTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_ROUTE_TOOL_H_
