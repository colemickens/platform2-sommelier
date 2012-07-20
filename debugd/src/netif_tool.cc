// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "netif_tool.h"

#include <string>

#include <dbus-c++/dbus.h>

#include "process_with_output.h"

namespace debugd {

NetifTool::NetifTool() { }

NetifTool::~NetifTool() { }

std::string NetifTool::GetInterfaces(DBus::Error* error) {
  char *envvar = getenv("DEBUGD_HELPERS");
  std::string path = StringPrintf("%s/netif", envvar ? envvar
                                  : "/usr/libexec/debugd/helpers");
  if (path.length() > PATH_MAX)
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
