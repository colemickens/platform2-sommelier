// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/route_tool.h"

#include <brillo/process.h>

#include "debugd/src/process_with_output.h"

namespace debugd {

namespace {

const char kIpTool[] = "/bin/ip";

}  // namespace

std::vector<std::string> RouteTool::GetRoutes(
    const brillo::VariantDictionary& options) {
  std::vector<std::string> result;
  ProcessWithOutput p;
  if (!p.Init())
    return result;
  p.AddArg(kIpTool);

  if (brillo::GetVariantValueOrDefault<bool>(options, "v6"))
    p.AddArg("-6");
  p.AddArg("r");  // route
  p.AddArg("s");  // show

  if (p.Run())
    return result;
  p.GetOutputLines(&result);
  return result;
}

}  // namespace debugd
