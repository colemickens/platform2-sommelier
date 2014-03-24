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
#include <gflags/gflags.h>

#include "buffet/dbus_manager.h"

DEFINE_string(logsroot, "/var/log", "Root directory for buffet logs.");

namespace {

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

// Creates new log file based on timestamp in |logs_root|/buffet.
std::string SetupLogFile(const std::string& logs_root) {
  const auto log_symlink = logs_root + "/buffet.log";
  const auto logs_dir = logs_root + "/buffet";
  const auto log_path =
      base::StringPrintf("%s/buffet.%s",
                         logs_dir.c_str(),
                         GetTimeAsString(::time(NULL)).c_str());
  mkdir(logs_dir.c_str(), 0755);
  SetupLogSymlink(log_symlink, log_path);
  return log_symlink;
}

// Sets up logging for buffet.
void SetupLogging(const std::string& logs_root) {
  const auto log_file = SetupLogFile(logs_root);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_file.c_str();
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;
  logging::InitLogging(settings);
}

}  // namespace

int main(int argc, char* argv[]) {
  // Parse the args and check for extra args.
  CommandLine::Init(argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK_EQ(argc, 1) << "Unexpected arguments. Try --help";

  SetupLogging(FLAGS_logsroot);

  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  // Initialize the dbus_manager.
  buffet::DBusManager dbus_manager;
  dbus_manager.Init();

  message_loop.Run();

  dbus_manager.Finalize();
  return 0;
}
