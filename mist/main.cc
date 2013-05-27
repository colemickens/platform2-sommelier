// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/string_number_conversions.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>

using std::string;

namespace {

namespace switches {

// Command line switch to show the help message and exit.
const char kHelp[] = "help";
// Command line switch to set the logging level:
//   0 = LOG(INFO), 1 = LOG(WARNING), 2 = LOG(ERROR)
const char kLogLevel[] = "log-level";
// Help message to show when the --help command line switch is specified.
const char kHelpMessage[] =
    "Chromium OS Modem Interface Switching Tool\n"
    "\n"
    "Available Switches:\n"
    "  --log-level=N\n"
    "    Logging level:\n"
    "      0: LOG(INFO), 1: LOG(WARNING), 2: LOG(ERROR)\n"
    "      -1: VLOG(1), -2: VLOG(2), etc\n"
    "  --help\n"
    "    Show this help.\n"
    "\n";

}  // namespace switches

int GetLogLevel(const string& log_level_value) {
  int log_level = 0;
  if (!base::StringToInt(log_level_value, &log_level)) {
    LOG(WARNING) << "Invalid log level '" << log_level_value << "'";
  }
  return log_level;
}

}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  chromeos::InitLog(chromeos::kLogToStderr);
  int log_level = cl->HasSwitch(switches::kLogLevel) ?
      GetLogLevel(cl->GetSwitchValueASCII(switches::kLogLevel)) : 0;
  logging::SetMinLogLevel(log_level);

  return 0;
}
