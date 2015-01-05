// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/route_tool.h"

#include <base/files/file_util.h>
#include <chromeos/process.h>

#include "debugd/src/process_with_output.h"

namespace debugd {

const char* kIpTool = "/bin/ip";

std::vector<std::string> RouteTool::GetRoutes(
    const std::map<std::string, DBus::Variant>& options, DBus::Error* error) {
  std::vector<std::string> result;
  ProcessWithOutput p;
  if (!p.Init())
    return result;
  p.AddArg(kIpTool);
  auto option_iter = options.find("v6");
  // Casting a variant converts it to the proper type; if it's actually a
  // different type the caller will get org.freedesktop.DBus.Error.InvalidArgs.
  if (option_iter != options.end() && static_cast<bool>(option_iter->second))
    p.AddArg("-6");
  p.AddArg("r");  // route
  p.AddArg("s");  // show
  if (p.Run())
    return result;
  p.GetOutputLines(&result);
  return result;
}

}  // namespace debugd
