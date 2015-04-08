// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/flag_helper.h>
#include <chromeos/syslog_logging.h>

#include "germ/germ_client.h"

namespace {
const char kShellExecutablePath[] = "/bin/sh";
}  // namespace

int main(int argc, char** argv) {
  DEFINE_string(name, "", "Name of the service, cannot be empty");
  DEFINE_bool(launch, false, "Start a service");
  DEFINE_bool(terminate, false, "Terminate a service");
  DEFINE_bool(interactive, false, "Run in the foreground");
  DEFINE_bool(shell, false,
              "Don't actually run the service, just launch a shell");
  DEFINE_int32(pid, 0, "Pid of the service to terminate");

  chromeos::FlagHelper::Init(argc, argv,
                             "germ [OPTIONS] -- <EXECUTABLE> [ARGUMENTS...]");
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);

  if (FLAGS_launch && FLAGS_terminate) {
    LOG(ERROR) << "--launch and --terminate are mutually exclusive";
    return 1;
  }

  // Default to Launch.
  if (!FLAGS_launch && !FLAGS_terminate) {
    FLAGS_launch = true;
  }

  int ret = 0;
  if (FLAGS_launch) {
    std::vector<std::string> args =
        base::CommandLine::ForCurrentProcess()->GetArgs();
    // It would be great if we could print the "Usage" message here,
    // but chromeos::FlagHelper does not seem to support that.
    // Instead, we LOG(ERROR) and exit. We don't LOG(FATAL) because we
    // don't need a backtrace or core dump.
    if (args.empty()) {
      LOG(ERROR) << "No executable file provided";
      return 1;
    }
    if (FLAGS_name.empty()) {
      LOG(ERROR) << "Empty service name";
      return 1;
    }

    // FLAGS_shell implies FLAGS_interactive.
    if (FLAGS_interactive || FLAGS_shell) {
      base::AtExitManager exit_manager;
      germ::Launcher launcher;
      if (FLAGS_shell) {
        args.clear();
        args.push_back(kShellExecutablePath);
      }
      int status = 0;
      bool success = launcher.RunInteractive(FLAGS_name, args, &status);
      if (!success) {
        LOG(ERROR) << "Failed to launch '" << FLAGS_name << "'";
        return 1;
      }
      ret = status;
    } else {
      germ::GermClient client;
      ret = client.Launch(FLAGS_name, args);
    }
  } else if (FLAGS_terminate) {
    if (FLAGS_pid <= 0) {
      LOG(ERROR) << "No/invalid pid provided";
      return 1;
    }
    germ::GermClient client;
    ret = client.Terminate(FLAGS_pid);
  }
  return ret;
}
