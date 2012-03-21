// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "systrace_tool.h"

#include <string>
#include <base/string_split.h>

#include <chromeos/process.h>

#include "process_with_output.h"

namespace debugd {

SystraceTool::SystraceTool() { }
SystraceTool::~SystraceTool() { }

static std::string getpathname(void) {
  char *envvar = getenv("DEBUGD_HELPERS");
  return StringPrintf("%s/systrace.sh", envvar ? envvar
                      : "/usr/libexec/debugd/helpers");
}

static void add_category_args(ProcessWithOutput& p,
    const std::string& categories)
{
  std::string temp(categories);
  std::vector<std::string> pieces;
  base::SplitString(temp, ' ', &pieces);
  for (std::vector<std::string>::iterator it = pieces.begin();
      it < pieces.end();
      it++)
    p.AddArg(*it);
}

std::string SystraceTool::Start(const std::string& categories,
                                DBus::Error& error) {
  ProcessWithOutput p;
  p.Init();
  p.AddArg(getpathname());
  p.AddArg("start");
  add_category_args(p, categories);
  p.Run();
  std::string out;
  p.GetOutput(&out);
  return out;
}

void SystraceTool::Stop(const DBus::FileDescriptor& outfd,
    DBus::Error& error) {
  ProcessWithOutput p;
  p.Init();
  p.AddArg(getpathname());
  p.AddArg("stop");
  // trace data is sent to stdout and not across dbus
  p.BindFd(outfd.get(), STDOUT_FILENO);
  p.Run();
}

std::string SystraceTool::Status(DBus::Error& error) {
  ProcessWithOutput p;
  p.Init();
  p.AddArg(getpathname());
  p.AddArg("status");
  p.Run();
  std::string out;
  p.GetOutput(&out);
  return out;
}

};  // namespace debugd
