// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/file_util.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/async_event_sequencer.h>
#include <chromeos/exported_object_manager.h>
#include <dbus/bus.h>
#include <sysexits.h>

#include "buffet/dbus_constants.h"
#include "buffet/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;

namespace {

static const char kLogRoot[] = "logroot";
static const char kHelp[] = "help";
static const char kDefaultLogRoot[] = "/var/log";

// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] = "\n"
    "Available Switches: \n"
    "  --logroot=/path/to/logroot\n"
    "    Specifies parent directory to put buffet logs in.\n";

// Returns |utime| as a string
std::string GetTimeAsString(time_t utime) {
  struct tm tm;
  CHECK_EQ(localtime_r(&utime, &tm), &tm);
  char str[16];
  CHECK_EQ(strftime(str, sizeof(str), "%Y%m%d-%H%M%S", &tm), 15UL);
  return std::string(str);
}

// Sets up a symlink to point to log file.
void SetupLogSymlink(const std::string& symlink_path,
                     const std::string& log_path) {
  base::DeleteFile(base::FilePath(symlink_path), true);
  if (symlink(log_path.c_str(), symlink_path.c_str()) == -1) {
    LOG(ERROR) << "Unable to create symlink " << symlink_path
               << " pointing at " << log_path;
  }
}

// Creates new log file based on timestamp in |log_root|/buffet.
std::string SetupLogFile(const std::string& log_root) {
  const auto log_symlink = log_root + "/buffet.log";
  const auto logs_dir = log_root + "/buffet";
  const auto log_path =
      base::StringPrintf("%s/buffet.%s",
                         logs_dir.c_str(),
                         GetTimeAsString(::time(NULL)).c_str());
  mkdir(logs_dir.c_str(), 0755);
  SetupLogSymlink(log_symlink, log_path);
  return log_symlink;
}

// Sets up logging for buffet.
void SetupLogging(const std::string& log_root) {
  const auto log_file = SetupLogFile(log_root);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_file.c_str();
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;
  logging::InitLogging(settings);
}

void TakeServiceOwnership(const scoped_refptr<dbus::Bus>& bus, bool success) {
  // Success should always be true since we've said that failures are
  // fatal.
  CHECK(success) << "Init of one or more objects has failed.";
  CHECK(bus->RequestOwnershipAndBlock(buffet::dbus_constants::kServiceName,
                                      dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << buffet::dbus_constants::kServiceName;
}

void EnterMainLoop(base::MessageLoopForIO* message_loop,
                   const scoped_refptr<dbus::Bus>& bus) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  ExportedObjectManager object_manager(
      bus, dbus::ObjectPath(buffet::dbus_constants::kRootServicePath));
  buffet::Manager manager(object_manager.AsWeakPtr());
  object_manager.Init(
      sequencer->GetHandler("ObjectManager.Init() failed.", true));
  manager.Init(sequencer->GetHandler("Manager.Init() failed.", true));
  sequencer->OnAllTasksCompletedCall(
      {base::Bind(&TakeServiceOwnership, bus)});
  // Release our handle on the sequencer so that it gets deleted after
  // both callbacks return.
  sequencer = nullptr;

  LOG(INFO) << "Entering mainloop.";
  message_loop->Run();
}

}  // namespace

int main(int argc, char* argv[]) {
  // Parse the args and check for extra args.
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(kHelp)) {
    LOG(INFO) << kHelpMessage;
    return EX_USAGE;
  }

  std::string log_root = std::string(kDefaultLogRoot);
  if (cl->HasSwitch(kLogRoot)) {
    log_root = cl->GetSwitchValueASCII(kLogRoot);
  }

  SetupLogging(log_root);

  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  dbus::Bus::Options options;
  // TODO(sosa): Should this be on the system bus?
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  CHECK(bus->Connect());
  // Our top level objects expect the bus to exist in a connected state for
  // the duration of their lifetimes.
  EnterMainLoop(&message_loop, bus);
  bus->ShutdownAndBlock();

  return EX_OK;
}
