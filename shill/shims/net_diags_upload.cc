// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shims/net_diags_upload.h"

#include <stdlib.h>

#include <string>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <chromeos/syslog_logging.h>

using std::string;

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

namespace shims {

void StashLogs() {
  // Captures the last 10000 lines in the log regardless of log rotation. I.e.,
  // prints the log files in timestamp sorted order and gets the tail of the
  // output.
  string cmdline =
      base::StringPrintf("/bin/cat $(/bin/ls -rt /var/log/net.*log) | "
                         "/bin/tail -10000 > %s", kStashedNetLog);
  if (system(cmdline.c_str())) {
    LOG(ERROR) << "Unable to stash net.log.";
  } else {
    LOG(INFO) << "net.log stashed.";
  }
}

}  // namespace shims

}  // namespace shill

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);

  shill::shims::StashLogs();

  if (cl->HasSwitch(switches::kUpload)) {
    // Crash so that crash_reporter can upload the logs.
    abort();
  }

  return 0;
}
