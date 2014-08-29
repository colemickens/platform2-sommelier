// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/netif_tool.h"

#include "debugd/src/process_with_output.h"

namespace debugd {

std::string NetifTool::GetInterfaces(DBus::Error* error) {
  std::string path;
  if (!SandboxedProcess::GetHelperPath("netif", &path))
    return "<path too long>";

  ProcessWithOutput p;
  if (!p.Init())
    return "<can't create process>";
  p.AddArg(path);
  p.Run();
  std::string out;
  p.GetOutput(&out);
  return out;
}

}  // namespace debugd
