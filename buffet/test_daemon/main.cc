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

#include "buffet/dbus-proxies.h"

class Daemon : public chromeos::DBusDaemon {
 public:
  Daemon() = default;

 protected:
  int OnInit() override;
  void OnShutdown(int* return_code) override;

 private:
  std::unique_ptr<org::chromium::Buffet::ObjectManagerProxy> object_manager_;

  void OnBuffetCommand(org::chromium::Buffet::CommandProxy* command);
  void OnBuffetCommandRemoved(const dbus::ObjectPath& object_path);
  void OnPropertyChange(org::chromium::Buffet::CommandProxy* command,
                        const std::string& property_name);
  void OnCommandProgress(org::chromium::Buffet::CommandProxy* command,
                         int progress);

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

int Daemon::OnInit() {
  int return_code = chromeos::DBusDaemon::OnInit();
  if (return_code != EX_OK)
    return return_code;

  object_manager_.reset(new org::chromium::Buffet::ObjectManagerProxy{bus_});
  object_manager_->SetCommandAddedCallback(
      base::Bind(&Daemon::OnBuffetCommand, base::Unretained(this)));
  object_manager_->SetCommandRemovedCallback(
      base::Bind(&Daemon::OnBuffetCommandRemoved, base::Unretained(this)));

  printf("Waiting for commands...\n");
  return EX_OK;
}

void Daemon::OnShutdown(int* return_code) {
  printf("Shutting down...\n");
}

void Daemon::OnPropertyChange(org::chromium::Buffet::CommandProxy* command,
                              const std::string& property_name) {
  printf("Notification: property '%s' on command '%s' changed.\n",
         property_name.c_str(), command->id().c_str());
  printf("  Current command status: '%s' (%d)\n",
         command->status().c_str(), command->progress());
}

void Daemon::OnBuffetCommand(org::chromium::Buffet::CommandProxy* command) {
  command->SetPropertyChangedCallback(base::Bind(&Daemon::OnPropertyChange,
                                                 base::Unretained(this)));
  printf("++++++++++++++++++++++++++++++++++++++++++++++++\n");
  printf("Command received: %s\n", command->name().c_str());
  printf("DBus Object Path: %s\n", command->GetObjectPath().value().c_str());
  printf("        category: %s\n", command->category().c_str());
  printf("              ID: %s\n", command->id().c_str());
  printf("          status: %s\n", command->status().c_str());
  printf(" # of parameters: %" PRIuS "\n", command->parameters().size());
  auto keys = chromeos::GetMapKeysAsVector(command->parameters());
  std::string param_names = chromeos::string_utils::Join(", ", keys);
  printf(" parameter names: %s\n", param_names.c_str());
  OnCommandProgress(command, 0);
}

void Daemon::OnCommandProgress(org::chromium::Buffet::CommandProxy* command,
                               int progress) {
  if (progress >= 100) {
    command->Done(nullptr);
  } else {
    printf("Updating command '%s' progress to %d%%\n",
           command->id().c_str(), progress);
    command->SetProgress(progress, nullptr);
    progress += 10;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&Daemon::OnCommandProgress,
                   base::Unretained(this),
                   command,
                   progress),
        base::TimeDelta::FromSeconds(1));
  }
}

void Daemon::OnBuffetCommandRemoved(const dbus::ObjectPath& object_path) {
  printf("------------------------------------------------\n");
  printf("Command removed\n");
  printf("DBus Object Path: %s\n", object_path.value().c_str());
}

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog |
                    chromeos::kLogToStderr |
                    chromeos::kLogHeader);
  Daemon daemon;
  return daemon.Run();
}
