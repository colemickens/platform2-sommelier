// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ROUTE_TOOL_H
#define ROUTE_TOOL_H

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class RouteTool {
 public:
  RouteTool();
  ~RouteTool();

  std::vector<std::string> GetRoutes(const std::map<std::string,
                                                     DBus::Variant>& options,
                                      DBus::Error& error);
 private:
  DISALLOW_COPY_AND_ASSIGN(RouteTool);
};

};  // namespace debugd

#endif  // ROUTE_TOOL_H
