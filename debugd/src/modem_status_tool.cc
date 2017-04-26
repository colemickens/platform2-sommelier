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

  string path;
  if (!SandboxedProcess::GetHelperPath("send_at_command.sh", &path))
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
  for (auto ch : input) {
    if (ch != '\n' && ch != '\r') {
      collapsing = false;
      out += ch;
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

}  // namespace debugd
