// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modem_status_tool.h"

#include <base/logging.h>

#include "process_with_output.h"

using base::StringPrintf;
using std::string;

namespace debugd {

ModemStatusTool::ModemStatusTool() { }
ModemStatusTool::~ModemStatusTool() { }

std::string ModemStatusTool::GetModemStatus(DBus::Error& error) { // NOLINT
  if (!USE_CELLULAR)
    return "";

  char *envvar = getenv("DEBUGD_HELPERS");
  std::string path = StringPrintf("%s/modem_status", envvar ? envvar
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

string ModemStatusTool::RunModemCommand(const string& command) {
  if (!USE_CELLULAR)
    return "";

  if (command == "get-oma-status") {
    return SendATCommand("AT+OMADM=?");
  }
  if (command == "start-oma") {
    string out = SendATCommand("AT+OMADM=1");
    out += SendATCommand("AT+OMADM=2");
    return out;
  }
  if (command == "get-prl") {
    return SendATCommand("AT$PRL?");
  }
  if (command == "ciprl-update") {
    string out = SendATCommand("AT+PRL=1");
    out += SendATCommand("AT+PRL=2");
    return out;
  }
  if (command == "get-service") {
    return SendATCommand("AT+SERVICE?");
  }
  return "ERROR: Unknown command: \"" + command + "\"";
}

string ModemStatusTool::SendATCommand(const string& command) {
  if (!USE_CELLULAR)
    return "";

  char *envvar = getenv("DEBUGD_HELPERS");
  string path = StringPrintf("%s/send_at_command.sh",
                             envvar ? envvar : "/usr/libexec/debugd/helpers");
  if (path.length() > PATH_MAX)
    return "";
  ProcessWithOutput p;
  p.SandboxAs("root", "root");
  p.Init();
  p.AddArg(path);
  p.AddArg(command);
  p.Run();
  string out;
  p.GetOutput(&out);
  return CollapseNewLines(out);
}

// static
string ModemStatusTool::CollapseNewLines(const string& input) {
  string out;
  bool collapsing = false;
  for (string::const_iterator it = input.begin(); it != input.end(); ++it) {
    if (*it != '\n' && *it != '\r') {
      collapsing = false;
      out += *it;
      continue;
    }
    if (collapsing) {
      continue;
    }
    out += '\n';
    collapsing = true;
  }
  return out;
}

};  // namespace debugd
