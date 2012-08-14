// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "icmp_tool.h"

#include <stdlib.h>
#include <base/stringprintf.h>

#include "process_with_output.h"

namespace debugd {

ICMPTool::ICMPTool() { }
ICMPTool::~ICMPTool() { }

std::string ICMPTool::TestICMP(const std::string& host, DBus::Error* error) {
  char *envvar = getenv("DEBUGD_HELPERS");
  std::string path = StringPrintf("%s/icmp", envvar ? envvar
                                  : "/usr/libexec/debugd/helpers");
  if (path.length() > PATH_MAX)
    return "<path too long>";
  ProcessWithOutput p;
  if (!p.Init())
    return "<can't create process>";
  p.AddArg(path);
  p.AddArg(host);
  p.Run();
  std::string out;
  p.GetOutput(&out);
  return out;
}

};  // namespace debugd
