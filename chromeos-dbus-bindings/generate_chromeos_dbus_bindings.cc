// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/command_line.h>
#include <base/logging.h>

namespace switches {

static const char kHelp[] = "help";
static const char kInput[] = "input";
static const char kHelpMessage[] = "\n"
    "Available Switches: \n"
    "  --input=<interface>\n"
    "    The input XML interface file (mandatory).\n";

}  // namespace switches

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  if (!cl->HasSwitch(switches::kInput)) {
    LOG(ERROR) << switches::kInput << " switch is mandatory.";
    LOG(ERROR) << switches::kHelpMessage;
    return 1;
  }

  std::string input = cl->GetSwitchValueASCII(switches::kInput);

  return 0;
}
