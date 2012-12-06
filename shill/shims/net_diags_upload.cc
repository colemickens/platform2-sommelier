// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>

namespace switches {

static const char kHelp[] = "help";
static const char kUpload[] = "upload";

// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] = "\n"
    "Available Switches:\n"
    "  --help\n"
    "    Show this help message.\n"
    "  --upload\n"
    "    Upload the diagnostics logs.\n";

}  // namespace switches

namespace shill {

void StashLogs() {
  // Captures the last 10000 lines in the log regardless of log rotation. I.e.,
  // prints the log files in timestamp sorted order and gets the tail of the
  // output.
  if (system("/bin/cat $(/bin/ls -rt /var/log/net.*log) | /bin/tail -10000 > "
             "/var/log/net-diags.net.log")) {
    LOG(ERROR) << "Unable to stash net.log.";
  } else {
    LOG(INFO) << "net.log stashed.";
  }
}

}  // namespace shill

int main(int argc, char **argv) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);

  shill::StashLogs();

  if (cl->HasSwitch(switches::kUpload)) {
    // Crash so that crash_reporter can upload the logs.
    abort();
  }

  return 0;
}
