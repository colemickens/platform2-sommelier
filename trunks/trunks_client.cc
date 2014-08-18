// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/message_loop/message_loop.h>

#include "trunks/command_transceiver.h"
#include "trunks/trunks_proxy.h"

void PrintString(const std::string& text) {
  LOG(INFO) << "Trunks Client: printing string: " << text;
  exit(0);
}

// trunks_client is a command line tool that allows us to
// send a command to the TPM and view its response.
// NOTE: At the moment it simply acts like an echo client
// with TrunksService being the echo server.
int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
  CommandLine *cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch("help")) {
    puts("Trunks Client: A command line tool to access the TPM.\n");
    puts("Options:\n");
    puts("  --help - Repeats this message\n");
    puts("  --command - command to send to the TPM(unimplemented)\n");
    return 0;
  }
  std::string command_text;
  if (cl->HasSwitch("command")) {
    command_text = cl->GetSwitchValueASCII("command");
  }

  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;
  trunks::TrunksProxy proxy;
  if (!proxy.Init()) {
    LOG(ERROR) << "Trunks Client: Error initializing TrunksProxy.";
    return -1;
  }
  proxy.SendCommand(command_text, base::Bind(PrintString));

  message_loop.Run();
  return -1;
}
