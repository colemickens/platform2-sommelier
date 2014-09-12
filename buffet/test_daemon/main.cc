// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a sample daemon that "handles" Buffet commands.
// It just prints the information about the command received to stdout and
// marks the command as processed.

#include <string>
#include <sysexits.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/format_macros.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/map_utils.h>
#include <chromeos/strings/string_utils.h>
#include <chromeos/syslog_logging.h>

#include "buffet/libbuffet/command.h"
#include "buffet/libbuffet/command_listener.h"

class Daemon : public chromeos::DBusDaemon {
 public:
  Daemon() = default;

 protected:
  int OnInit() override;
  void OnShutdown(int* return_code) override;

 private:
  buffet::CommandListener command_listener_;

  void OnBuffetCommand(scoped_ptr<buffet::Command> command);

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

int Daemon::OnInit() {
  int return_code = chromeos::DBusDaemon::OnInit();
  if (return_code != EX_OK)
    return return_code;

  if (!command_listener_.Init(bus_, base::Bind(&Daemon::OnBuffetCommand,
                                               base::Unretained(this)))) {
    return EX_SOFTWARE;
  }

  printf("Waiting for commands...\n");
  return EX_OK;
}

void Daemon::OnShutdown(int* return_code) {
  printf("Shutting down...\n");
}

void Daemon::OnBuffetCommand(scoped_ptr<buffet::Command> command) {
  printf("================================================\n");
  printf("Command received: %s\n", command->GetName().c_str());
  printf("        category: %s\n", command->GetCategory().c_str());
  printf("              ID: %s\n", command->GetID().c_str());
  printf("          status: %s\n", command->GetStatus().c_str());
  printf(" # of parameters: %" PRIuS "\n", command->GetParameters().size());
  auto set = chromeos::GetMapKeys(command->GetParameters());
  std::string param_names = chromeos::string_utils::Join(
      ", ", std::vector<std::string>(set.begin(), set.end()));
  printf(" parameter names: %s\n", param_names.c_str());
  command->Done();
}

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog |
                    chromeos::kLogToStderr |
                    chromeos::kLogHeader);
  Daemon daemon;
  return daemon.Run();
}
