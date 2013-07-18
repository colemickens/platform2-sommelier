// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "systrace_tool.h"

#include <string>
#include <base/strings/string_split.h>

#include <chromeos/process.h>

#include "process_with_output.h"

using base::StringPrintf;

namespace debugd {

namespace {

const char kSystraceHelper[] = "systrace.sh";

}  // namespace

extern const char *kDebugfsGroup;

SystraceTool::SystraceTool() { }
SystraceTool::~SystraceTool() { }

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
  std::string path;
  if (!SandboxedProcess::GetHelperPath(kSystraceHelper, &path))
    return "";

  ProcessWithOutput p;
  // this tool needs to reach into /sys/kernel/debug to enable/disable tracing
  p.SandboxAs(SandboxedProcess::kDefaultUser, kDebugfsGroup);
  p.Init();
  p.AddArg(path);
  p.AddArg("start");
  add_category_args(p, categories);
  p.Run();
  std::string out;
  p.GetOutput(&out);
  return out;
}

void SystraceTool::Stop(const DBus::FileDescriptor& outfd,
    DBus::Error& error) {
  std::string path;
  if (!SandboxedProcess::GetHelperPath(kSystraceHelper, &path))
    return;

  ProcessWithOutput p;
  p.SandboxAs(SandboxedProcess::kDefaultUser, kDebugfsGroup);
  p.Init();
  p.AddArg(path);
  p.AddArg("stop");
  // trace data is sent to stdout and not across dbus
  p.BindFd(outfd.get(), STDOUT_FILENO);
  p.Run();
}

std::string SystraceTool::Status(DBus::Error& error) {
  std::string path;
  if (!SandboxedProcess::GetHelperPath(kSystraceHelper, &path))
    return "";

  ProcessWithOutput p;
  p.SandboxAs(SandboxedProcess::kDefaultUser, kDebugfsGroup);
  p.Init();
  p.AddArg(path);
  p.AddArg("status");
  p.Run();
  std::string out;
  p.GetOutput(&out);
  return out;
}

};  // namespace debugd
