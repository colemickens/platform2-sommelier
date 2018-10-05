// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/modem_status_tool.h"

#include "debugd/src/process_with_output.h"

using std::string;

namespace debugd {

string ModemStatusTool::GetModemStatus() {
  if (!USE_CELLULAR)
    return "";

  string path;
  if (!SandboxedProcess::GetHelperPath("modem_status", &path))
    return "";

  ProcessWithOutput p;
  p.Init();
  p.AddArg(path);
  p.Run();
  string out;
  p.GetOutput(&out);
  return out;
}

}  // namespace debugd
