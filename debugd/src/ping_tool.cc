// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ping_tool.h"

#include <map>
#include <string>

#include "process_with_id.h"

namespace debugd {

const char* kPing = "/bin/ping";

PingTool::PingTool() : SubprocessTool() { }
PingTool::~PingTool() { }

std::string PingTool::Start(const DBus::FileDescriptor& outfd,
                            const std::string& destination,
                            const std::map<std::string, DBus::Variant>&
                                options,
                            DBus::Error& error) {
  ProcessWithId* p = CreateProcess();
  if (!p)
    return "";
  p->AddArg(kPing);
  if (options.count("count") == 1) {
    // If we try to convert a non-int value to an int here, dbus-c++ will toss
    // a C++ exception, which the dbus-c++ main loop will convert into a dbus
    // exception and return it to our caller.
    p->AddIntOption("-c", options.find("count")->second);
  } else {
    p->AddIntOption("-c", 4);
  }
  if (options.count("interval") == 1) {
    p->AddIntOption("-i", options.find("interval")->second);
  }
  if (options.count("numeric") == 1) {
    p->AddArg("-n");
  }
  if (options.count("packetsize") == 1) {
    p->AddIntOption("-s", options.find("packetsize")->second);
  }
  if (options.count("waittime") == 1) {
    p->AddIntOption("-W", options.find("waittime")->second);
  }
  p->AddArg(destination);
  p->BindFd(outfd.get(), STDOUT_FILENO);
  p->BindFd(outfd.get(), STDERR_FILENO);
  LOG(INFO) << "ping: running process id: " << p->id();
  p->Start();
  return p->id();
}

};  // namespace debugd
