// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/network_status_tool.h"

#include <base/logging.h>

#include "debugd/src/process_with_output.h"

using base::StringPrintf;

namespace debugd {

NetworkStatusTool::NetworkStatusTool() { }
NetworkStatusTool::~NetworkStatusTool() { }

std::string NetworkStatusTool::GetNetworkStatus(DBus::Error* error) {
  std::string path;
  if (!SandboxedProcess::GetHelperPath("network_status", &path))
    return "";

  ProcessWithOutput p;
  p.Init();
  p.AddArg(path);
  p.Run();
  std::string out;
  p.GetOutput(&out);
  return out;
}

}  // namespace debugd
