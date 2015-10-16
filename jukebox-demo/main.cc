// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a sample daemon that handles a few simple Weave commands.

#include <string>
#include <sysexits.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/syslog_logging.h>

#include "buffet/dbus-proxies.h"

using org::chromium::Buffet::CommandProxy;
using org::chromium::Buffet::ManagerProxy;

namespace {
// Helper function to get a command parameter of particular type T from the
// command parameter list. Returns default value for type T (e.g. 0 for int or
// or "" for std::string) if the parameter with the given name is not found or
// it is of incorrect type.
template <typename T>
T GetParameter(const brillo::VariantDictionary& parameters,
               const std::string& name) {
  T value{};
  auto p = parameters.find(name);
  if (p != parameters.end())
    p->second.GetValue<T>(&value);
  return value;
}
}  // anonymous namespace

class Daemon final : public brillo::DBusDaemon {
 public:
  Daemon() = default;

 protected:
  int OnInit() override;

 private:
  // Buffet's D-Bus object manager class that is used to communicate with Buffet
  // and receive Weave commands from local clients or GCD server.
  std::unique_ptr<org::chromium::Buffet::ObjectManagerProxy> object_manager_;

  // Device state variables.
  std::string song_name_;
  std::string status_{"idle"};
  int volume_{0};

  // Main callback invoked when new command is added to Buffet's command queue.
  void OnCommand(CommandProxy* command);

  // Particular command handlers for various commands.
  void OnPlay(CommandProxy* command);
  void OnStop(CommandProxy* command);
  void OnSetVolume(CommandProxy* command);
  void OnChangeVolume(CommandProxy* command);

  // Helper methods to propagate device state changes to Buffet and hence to
  // the cloud server or local clients.
  void NotifyDeviceStateChanged();
  void UpdateDeviceState(ManagerProxy* manager);

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

int Daemon::OnInit() {
  int return_code = brillo::DBusDaemon::OnInit();
  if (return_code != EX_OK)
    return return_code;

  object_manager_.reset(new org::chromium::Buffet::ObjectManagerProxy{bus_});
  object_manager_->SetCommandAddedCallback(
      base::Bind(&Daemon::OnCommand, base::Unretained(this)));
  object_manager_->SetManagerAddedCallback(
      base::Bind(&Daemon::UpdateDeviceState, base::Unretained(this)));

  LOG(INFO) << "Waiting for commands...";
  return EX_OK;
}

void Daemon::OnCommand(CommandProxy* command) {
  // Handle only commands that belong to this daemon's category.
  if (command->state() != "queued")
    return;

  LOG(INFO) << "Command: " << command->name();
  if (command->name() == "_jukebox._play") {
    OnPlay(command);
  } else if (command->name() == "_jukebox._stop") {
    OnStop(command);
  } else if (command->name() == "_jukebox._setVolume") {
    OnSetVolume(command);
  } else if (command->name() == "_jukebox._changeVolume") {
    OnChangeVolume(command);
  }
}

void Daemon::OnPlay(CommandProxy* command) {
  song_name_ = GetParameter<std::string>(command->parameters(), "_songName");
  status_ = "playing";
  NotifyDeviceStateChanged();
  CHECK(command->Complete({}, nullptr));
}

void Daemon::OnStop(CommandProxy* command) {
  song_name_.clear();
  status_ = "idle";
  NotifyDeviceStateChanged();
  CHECK(command->Complete({}, nullptr));
}

void Daemon::OnSetVolume(CommandProxy* command) {
  volume_ = GetParameter<int>(command->parameters(), "_volume");
  NotifyDeviceStateChanged();
  CHECK(command->Complete({}, nullptr));
}

void Daemon::OnChangeVolume(CommandProxy* command) {
  volume_ += GetParameter<int>(command->parameters(), "_delta");
  NotifyDeviceStateChanged();
  brillo::VariantDictionary results{{"_currentVolume", volume_}};
  CHECK(command->Complete(results, nullptr));
}

void Daemon::UpdateDeviceState(ManagerProxy* manager) {
  brillo::VariantDictionary state_change{
    {"_jukebox._volume", volume_},
    {"_jukebox._status", status_},
    {"_jukebox._songName", song_name_},
  };
  CHECK(manager->UpdateState(state_change, nullptr));
}

void Daemon::NotifyDeviceStateChanged() {
  ManagerProxy* manager = object_manager_->GetManagerProxy();
  if (manager)
    UpdateDeviceState(manager);
}

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  brillo::InitLog(brillo::kLogToSyslog |
                    brillo::kLogHeader);
  Daemon daemon;
  return daemon.Run();
}
