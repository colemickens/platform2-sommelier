// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>
#include <glib-object.h>
#include <glib-unix.h>

#include <base/at_exit.h>
#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <chromeos/syslog_logging.h>

#include "wimax_manager/event_dispatcher.h"
#include "wimax_manager/manager.h"
#include "wimax_manager/manager_dbus_adaptor.h"

using std::string;
using wimax_manager::EventDispatcher;

namespace {

namespace switches {

// Command line switch to run WiMAX manager in foreground.
const char kForeground[] = "foreground";
// Command line switch to show the help message and exit.
const char kHelp[] = "help";
// Command line switch to set the logging level:
//   0 = LOG(INFO), 1 = LOG(WARNING), 2 = LOG(ERROR)
const char kLogLevel[] = "log-level";
// Help message to show when the --help command line switch is specified.
const char kHelpMessage[] =
    "Chromium OS WiMAX Manager\n"
    "\n"
    "Available Switches:\n"
    "  --foreground\n"
    "    Do not daemonize; run in foreground.\n"
    "  --log-level=N\n"
    "    Logging level:\n"
    "      0: LOG(INFO), 1: LOG(WARNING), 2: LOG(ERROR)\n"
    "      -1: VLOG(1), -2: VLOG(2), etc\n"
    "  --help\n"
    "    Show this help.\n"
    "\n";

}  // namespace switches

int GetLogLevel(const string &log_level_value) {
  int log_level = 0;
  if (!base::StringToInt(log_level_value, &log_level)) {
    LOG(WARNING) << "Invalid log level '" << log_level_value << "'";
  } else if (log_level >= logging::LOG_NUM_SEVERITIES) {
    log_level = logging::LOG_NUM_SEVERITIES;
  }
  return log_level;
}

// Always logs to syslog and stderr when running in the foreground.
void SetupLogging(bool foreground, int log_level) {
  int log_flags = chromeos::kLogToSyslog;
  if (foreground)
    log_flags |= chromeos::kLogToStderr;

  chromeos::InitLog(log_flags);
  logging::SetMinLogLevel(log_level);
}

// This callback will be inovked when this process receives SIGINT or SIGTERM.
gboolean TerminationSignalCallback(gpointer data) {
  EventDispatcher *dispatcher = reinterpret_cast<EventDispatcher *>(data);
  dispatcher->Stop();

  // This function can return false to remove this signal handler as we are
  // quitting the dispatcher loop anyway.
  return FALSE;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  g_type_init();

  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  bool foreground = cl->HasSwitch(switches::kForeground);
  int log_level = cl->HasSwitch(switches::kLogLevel) ?
      GetLogLevel(cl->GetSwitchValueASCII(switches::kLogLevel)) : 0;

  SetupLogging(foreground, log_level);

  if (!foreground)
    PLOG_IF(FATAL, ::daemon(0, 0) == 1) << "Failed to create daemon";

  EventDispatcher dispatcher;

  // Set up a signal handler for handling SIGINT and SIGTERM.
  g_unix_signal_add(SIGINT, TerminationSignalCallback, &dispatcher);
  g_unix_signal_add(SIGTERM, TerminationSignalCallback, &dispatcher);

  wimax_manager::Manager manager(&dispatcher);
  manager.CreateDBusAdaptor();
  CHECK(manager.Initialize()) << "Failed to initialize WiMAX manager";

  dispatcher.DispatchForever();

  return 0;
}
