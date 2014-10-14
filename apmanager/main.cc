// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/minijail/minijail.h>
#include <chromeos/syslog_logging.h>

#include "apmanager/daemon.h"

using std::vector;

namespace {

namespace switches {

// Don't daemon()ize; run in foreground.
const char kForeground[] = "foreground";
// Flag that causes apmanager to show the help message and exit.
const char kHelp[] = "help";

// The help message shown if help flag is passed to the program.
const char kHelpMessage[] = "\n"
    "Available Switches: \n"
    "  --foreground\n"
    "    Don\'t daemon()ize; run in foreground.\n";
}  // namespace switches

}  // namespace

namespace {

const char kLoggerCommand[] = "/usr/bin/logger";
const char kLoggerUser[] = "syslog";

}  // namespace

// Always logs to the syslog and logs to stderr if
// we are running in the foreground.
void SetupLogging(chromeos::Minijail* minijail,
                  bool foreground,
                  const char* daemon_name) {
  int log_flags = 0;
  log_flags |= chromeos::kLogToSyslog;
  log_flags |= chromeos::kLogHeader;
  if (foreground) {
    log_flags |= chromeos::kLogToStderr;
  }
  chromeos::InitLog(log_flags);

  if (!foreground) {
    vector<char*> logger_command_line;
    int logger_stdin_fd;
    logger_command_line.push_back(const_cast<char*>(kLoggerCommand));
    logger_command_line.push_back(const_cast<char*>("--priority"));
    logger_command_line.push_back(const_cast<char*>("daemon.err"));
    logger_command_line.push_back(const_cast<char*>("--tag"));
    logger_command_line.push_back(const_cast<char*>(daemon_name));
    logger_command_line.push_back(nullptr);

    struct minijail* jail = minijail->New();
    minijail->DropRoot(jail, kLoggerUser, kLoggerUser);

    if (!minijail->RunPipeAndDestroy(jail, logger_command_line,
                                     nullptr, &logger_stdin_fd)) {
      LOG(ERROR) << "Unable to spawn logger. "
                 << "Writes to stderr will be discarded.";
      return;
    }

    // Note that we don't set O_CLOEXEC here. This means that stderr
    // from any child processes will, by default, be logged to syslog.
    if (dup2(logger_stdin_fd, fileno(stderr)) != fileno(stderr)) {
      LOG(ERROR) << "Failed to redirect stderr to syslog: "
                 << strerror(errno);
    }
    close(logger_stdin_fd);
  }
}

void DropPrivileges(chromeos::Minijail* minijail) {
  struct minijail* jail = minijail->New();
  minijail->DropRoot(jail, apmanager::Daemon::kAPManagerUserName,
                     apmanager::Daemon::kAPManagerGroupName);
  minijail_enter(jail);
  minijail->Destroy(jail);
}

void OnStartup(const char* daemon_name, CommandLine* cl) {
  chromeos::Minijail* minijail = chromeos::Minijail::GetInstance();
  SetupLogging(minijail, cl->HasSwitch(switches::kForeground), daemon_name);

  LOG(INFO) << __func__ << ": Dropping privileges";

  // Now that the daemon has all the resources it needs to run, we can drop
  // privileges further.
  DropPrivileges(minijail);
}

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  const int nochdir = 0, noclose = 0;
  if (!cl->HasSwitch(switches::kForeground))
    PLOG_IF(FATAL, daemon(nochdir, noclose) == -1) << "Failed to daemonize";

  apmanager::Daemon daemon(base::Bind(&OnStartup, argv[0], cl));

  daemon.Run();

  return 0;
}
