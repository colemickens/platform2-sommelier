//
// Copyright (C) 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <stdio.h>
#include <string>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <brillo/syslog_logging.h>

#include "trunks/trunks_dbus_proxy.h"

namespace {

using trunks::TrunksDBusProxy;

void PrintUsage() {
  puts("Usage: trunks_send <XX..XX>");
}

std::string HexEncode(const std::string& bytes) {
  return base::HexEncode(bytes.data(), bytes.size());
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  brillo::InitLog(brillo::kLogToStderr);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (cl->HasSwitch("help")) {
    puts("Trunks Send: sends a raw command to the TPM and prints the response.");
    PrintUsage();
    return 0;
  }

  TrunksDBusProxy proxy;
  if (!proxy.Init()) {
    LOG(ERROR) << "Failed to initialize dbus proxy.";
    return 1;
  }

  std::string hex_command;
  for (std::string arg : cl->GetArgs()) {
    hex_command.append(arg);
  }
  base::RemoveChars(hex_command, " \t\r\n:.", &hex_command);

  std::vector<uint8_t> bytes;
  if (!base::HexStringToBytes(hex_command, &bytes)) {
    LOG(ERROR) << "Can't convert input to bytes.";
    PrintUsage();
    return 2;
  }

  std::string command(bytes.data(), bytes.data() + bytes.size());
  if (command.empty()) {
    LOG(ERROR) << "Empty command.";
    PrintUsage();
    return 3;
  }

  std::string response = proxy.SendCommandAndWait(command);
  puts(HexEncode(response).c_str());

  return 0;
}
