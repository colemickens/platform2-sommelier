// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A daemon for performing crypto operations for Easy Unlock.

#include <base/at_exit.h>
#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/file_util.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/strings/string_number_conversions.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/syslog_logging.h>
#include <dbus/bus.h>

#include "easy-unlock/easy_unlock_service.h"
#include "easy-unlock/daemon.h"

namespace {

namespace switches {

// Command line switch to run this daemon in foreground.
const char kForeground[] = "foreground";

// Command line switch to show the help message and exit.
const char kHelp[] = "help";

// Command line switch to set the logging level:
//   0 = LOG(INFO), 1 = LOG(WARNING), 2 = LOG(ERROR)
const char kLogLevel[] = "log-level";

// Help message to show when the --help command line switch is specified.
const char kHelpMessage[] =
    "Chrome OS EasyUnlock Daemon\n"
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

int GetLogLevel(const std::string& log_level_value) {
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

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;

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

  base::MessageLoopForIO message_loop;
  base::RunLoop run_loop;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);

  scoped_ptr<easy_unlock::Service> service(easy_unlock::Service::Create());
  scoped_ptr<easy_unlock::Daemon> daemon(
      new easy_unlock::Daemon(service.Pass(),
      bus,
      run_loop.QuitClosure(),
      true /* install signal handler */));
  LOG_IF(FATAL, !daemon->Initialize());

  LOG(INFO) << "EasyUnlock dbus service started.";

  run_loop.Run();

  LOG(INFO) << "Cleaning up and exiting.";
  daemon->Finalize();

  return 0;
}
