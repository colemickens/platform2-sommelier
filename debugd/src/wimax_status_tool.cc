// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_status_tool.h"

#include <base/logging.h>

#include "process_with_output.h"

namespace debugd {

WiMaxStatusTool::WiMaxStatusTool() {}
WiMaxStatusTool::~WiMaxStatusTool() {}

std::string WiMaxStatusTool::GetWiMaxStatus(DBus::Error& error) {  // NOLINT
  if (!USE_WIMAX)
    return "";

  char *envvar = getenv("DEBUGD_HELPERS");
  std::string path = StringPrintf("%s/wimax_status", envvar ? envvar
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

}  // namespace debugd
