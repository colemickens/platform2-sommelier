// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "icmp_tool.h"

#include <stdlib.h>
#include <base/strings/stringprintf.h>

#include "process_with_output.h"

using base::StringPrintf;
using std::map;
using std::string;

namespace debugd {

ICMPTool::ICMPTool() { }
ICMPTool::~ICMPTool() { }

string ICMPTool::TestICMP(const string& host, DBus::Error* error) {
  map<string, string> options;
  return TestICMPWithOptions(host, options, error);
}

string ICMPTool::TestICMPWithOptions(const string& host,
                                     const map<string, string>& options,
                                     DBus::Error* error) {
  string path;
  if (!SandboxedProcess::GetHelperPath("icmp", &path))
    return "<path too long>";

  ProcessWithOutput p;
  if (!p.Init())
    return "<can't create process>";
  p.AddArg(path);

  for (map<string, string>::const_iterator it = options.begin();
       it != options.end();
       it++) {
    // No need to quote here because chromeos:ProcessImpl (base class of
    // ProcessWithOutput) passes arguments as is to helpers/icmp, which will
    // check arguments before executing in the shell.
    p.AddArg(StringPrintf("--%s=%s", it->first.c_str(), it->second.c_str()));
  }

  p.AddArg(host);
  p.Run();
  string out;
  p.GetOutput(&out);
  return out;
}

};  // namespace debugd
