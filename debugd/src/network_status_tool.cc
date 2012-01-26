// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "network_status_tool.h"

#include <base/logging.h>

#include "process_with_output.h"

namespace debugd {

NetworkStatusTool::NetworkStatusTool() { }
NetworkStatusTool::~NetworkStatusTool() { }

std::string NetworkStatusTool::GetNetworkStatus(DBus::Error& error) { // NOLINT
  char *envvar = getenv("DEBUGD_HELPERS");
  std::string path = StringPrintf("%s/network_status", envvar ? envvar
                                  : "/usr/libexec/debugd/helpers");
  if (path.length() > PATH_MAX)
    return "";
  ProcessWithOutput p;
  p.Init();
  p.AddArg(path);
  p.Run();
  std::string out;
  p.GetOutput(&out);
  return out;
}

};  // namespace debugd
