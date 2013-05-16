// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <glib-unix.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <chromeos/syslog_logging.h>

#include "lorgnette/daemon.h"
#include "lorgnette/minijail.h"

using base::FilePath;
using std::string;
using std::vector;

namespace switches {

// Don't daemon()ize; run in foreground.
static const char kForeground[] = "foreground";
// Flag that causes lorgnette to show the help message and exit.
static const char kHelp[] = "help";

// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] = "\n"
    "Available Switches: \n"
    "  --foreground\n"
    "    Don\'t daemon()ize; run in foreground.\n";
}  // namespace switches

namespace {

const char *kLoggerCommand = "/usr/bin/logger";
const char *kLoggerUser = "syslog";

}  // namespace

// Always logs to the syslog and logs to stderr if
// we are running in the foreground.
void SetupLogging(bool foreground, char *daemon_name) {
  int log_flags = 0;
  log_flags |= chromeos::kLogToSyslog;
  log_flags |= chromeos::kLogHeader;
  if (foreground) {
    log_flags |= chromeos::kLogToStderr;
  }
  chromeos::InitLog(log_flags);

  if (!foreground) {
    vector<char *> logger_command_line;
    int logger_stdin_fd;
    logger_command_line.push_back(const_cast<char *>(kLoggerCommand));
    logger_command_line.push_back(const_cast<char *>("--priority"));
    logger_command_line.push_back(const_cast<char *>("daemon.err"));
    logger_command_line.push_back(const_cast<char *>("--tag"));
    logger_command_line.push_back(daemon_name);
    logger_command_line.push_back(NULL);

    lorgnette::Minijail *minijail = lorgnette::Minijail::GetInstance();
    struct minijail *jail = minijail->New();
    minijail->DropRoot(jail, kLoggerUser, kLoggerUser);

    if (!minijail->RunPipeAndDestroy(jail, logger_command_line,
                                     NULL, &logger_stdin_fd)) {
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

gboolean ExitSigHandler(gpointer data) {
  LOG(INFO) << "Shutting down due to received signal.";
  lorgnette::Daemon *daemon = reinterpret_cast<lorgnette::Daemon*>(data);
  daemon->Quit();
  return TRUE;
}


int main(int argc, char **argv) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  CommandLine *cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  const int nochdir = 0, noclose = 0;
  if (!cl->HasSwitch(switches::kForeground))
    PLOG_IF(FATAL, daemon(nochdir, noclose) == -1 ) << "Failed to daemonize";

  SetupLogging(cl->HasSwitch(switches::kForeground), argv[0]);

  lorgnette::Daemon daemon;

  g_unix_signal_add(SIGINT, ExitSigHandler, &daemon);
  g_unix_signal_add(SIGTERM, ExitSigHandler, &daemon);

  daemon.Start();

  // Now that the daemon has all the resources it needs to run, we can drop
  // privileges further.
  struct minijail *jail = minijail_new();
  minijail_change_user(jail, lorgnette::Daemon::kScanUserName);
  minijail_change_group(jail, lorgnette::Daemon::kScanGroupName);
  minijail_enter(jail);

  daemon.Run();

  return 0;
}
