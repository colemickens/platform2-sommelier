// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "route_tool.h"

#include <base/file_util.h>
#include <chromeos/process.h>

#include "process_with_output.h"

namespace debugd {

const char* kRoute = "/sbin/route";

RouteTool::RouteTool() { }
RouteTool::~RouteTool() { }

std::vector<std::string> RouteTool::GetRoutes(const std::map<std::string,
                                                             DBus::Variant>&
                                                  options,
                                              DBus::Error& error) {
  std::vector<std::string> result;
  ProcessWithOutput p;
  if (!p.Init())
    return result;
  p.AddArg(kRoute);
  if (options.count("numeric") == 1)
    p.AddArg("-n");
  if (options.count("v6") == 1)
    p.AddStringOption("-A", "inet6");
  if (p.Run())
    return result;
  p.GetOutputLines(&result);
  return result;
}

};  // namespace debugd
