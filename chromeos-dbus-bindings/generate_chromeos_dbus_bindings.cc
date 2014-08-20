// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/logging.h>

#include "chromeos-dbus-bindings/xml_interface_parser.h"

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

  chromeos_dbus_bindings::XmlInterfaceParser parser;
  if (!parser.ParseXmlInterfaceFile(base::FilePath(input))) {
    LOG(ERROR) << "Failed to parse interface file.";
    return 1;
  }

  return 0;
}
