// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packet_capture_tool.h"

#include <base/strings/string_util.h>

#include "process_with_id.h"

namespace debugd {

PacketCaptureTool::PacketCaptureTool() : SubprocessTool() { }

PacketCaptureTool::~PacketCaptureTool() { }

std::string PacketCaptureTool::Start(
    const DBus::FileDescriptor& status_fd,
    const DBus::FileDescriptor& output_fd,
    const std::map<std::string, DBus::Variant>& options,
    DBus::Error& error) {
  char* envvar = getenv("DEBUGD_HELPERS");
  std::string exec_path = StringPrintf("%s/capture_utility.sh", envvar ? envvar
                                       : "/usr/libexec/debugd/helpers");
  if (exec_path.length() > PATH_MAX)
    return "<path too long>";
  ProcessWithId* p = CreateProcess(false);
  if (!p)
    return "<create process failed>";
  p->AddArg(exec_path);
  AddValidatedStringOption(options, "device", "--device", p);
  if (options.count("frequency") == 1) {
    // If we try to convert a non-int value to an int here, dbus-c++ will toss
    // a C++ exception, which the dbus-c++ main loop will convert into a dbus
    // exception and return it to our caller.
    p->AddIntOption("--frequency", options.find("frequency")->second);
  }
  AddValidatedStringOption(options, "ht_location", "--ht-location", p);
  AddValidatedStringOption(
      options, "monitor_connection_on","--monitor-connection-on", p);

  // Pass the output fd of the pcap as a command line option to the child
  // process.
  int child_output_fd = STDERR_FILENO + 1;
  p->AddStringOption("--output-file",
                     StringPrintf("/dev/fd/%d", child_output_fd));
  p->BindFd(output_fd.get(), child_output_fd);

  p->BindFd(status_fd.get(), STDOUT_FILENO);
  p->BindFd(status_fd.get(), STDERR_FILENO);
  LOG(INFO) << "packet_capture: running process id: " << p->id();
  p->Start();
  return p->id();
}

bool PacketCaptureTool::AddValidatedStringOption(
    const std::map<std::string, DBus::Variant>& options,
    const std::string &dbus_option,
    const std::string &command_line_option,
    ProcessWithId* p) {
  if (options.count(dbus_option) != 1) {
    return false;
  }
  const std::string &option_value = options.find(dbus_option)->second;
  for (size_t i = 0; i < option_value.length(); ++i) {
    const char c = option_value[i];
    // These are the only plausible interface name characters.
    if (!IsAsciiAlpha(c) && !IsAsciiDigit(c) && c != '-' && c != '_') {
      return false;
    }
  }

  p->AddStringOption(command_line_option, option_value);
  return true;
}

};  // namespace debugd
