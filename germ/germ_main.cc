// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <chromeos/flag_helper.h>
#include <chromeos/syslog_logging.h>

#include "germ/launcher.h"

namespace {
const char kShellExecutablePath[] = "/bin/sh";
}

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;

  DEFINE_string(name, "",
                "Name of the service, cannot be empty.");
  DEFINE_string(executable, "",
                "Full path of the executable, cannot be empty.");
  DEFINE_bool(interactive, false, "Run in the foreground.");
  DEFINE_bool(shell, false,
              "Don't actually run the service, just launch a shell. "
              "Implies --interactive.");

  chromeos::FlagHelper::Init(argc, argv, "germ");
  chromeos::InitLog(chromeos::kLogToSyslog);

  germ::Launcher launcher;
  int ret = 0;
  // FLAGS_shell implies FLAGS_interactive.
  if (FLAGS_interactive || FLAGS_shell) {
    std::string executable =
        FLAGS_shell ? std::string(kShellExecutablePath) : FLAGS_executable;
    ret = launcher.RunInteractive(FLAGS_name, executable);
  } else {
    ret = launcher.RunService(FLAGS_name, FLAGS_executable);
  }
  return ret;
}
