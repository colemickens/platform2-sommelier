// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <string>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>

#include "shill/dbus_control.h"
#include "shill/dhcp_provider.h"
#include "shill/shill_daemon.h"

using std::string;

namespace switches {

// Don't daemon()ize; run in foreground.
static const char kForeground[] = "foreground";
// Directory to read confguration settings.
static const char kConfigDir[] = "config-dir";
// Directory to read default configuration settings (Read Only).
static const char kDefaultConfigDir[] = "default-config-dir";
// Flag that causes shill to show the help message and exit.
static const char kHelp[] = "help";

// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] = "\n"
    "Available Switches: \n"
    "  --foreground\n"
    "    Don\'t daemon()ize; run in foreground.\n"
    "  --config_dir\n"
    "    Directory to read confguration settings.\n"
    "  --default_config_dir\n"
    "    Directory to read default configuration settings (Read Only).";
}  // namespace switches

// Always logs to the syslog and logs to stderr if
// we are running in the foreground.
void SetupLogging(bool foreground) {
  int log_flags = 0;
  log_flags |= chromeos::kLogToSyslog;
  if (foreground) {
    log_flags |= chromeos::kLogToStderr;
  }
  chromeos::InitLog(log_flags);
}


int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  // If the help flag is set, force log in foreground.
  SetupLogging(cl->HasSwitch(switches::kForeground) ||
               cl->HasSwitch(switches::kHelp));
  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  FilePath config_dir(cl->GetSwitchValueASCII(switches::kConfigDir));
  FilePath default_config_dir(
      !cl->HasSwitch(switches::kDefaultConfigDir) ?
      shill::Config::kShillDefaultPrefsDir :
      cl->GetSwitchValueASCII(switches::kDefaultConfigDir));

  shill::Config config; /* (config_dir, default_config_dir) */

  // TODO(pstew): This should be chosen based on config
  shill::DBusControl *dbus_control = new shill::DBusControl();
  dbus_control->Init();
  shill::DHCPProvider::GetInstance()->Init(dbus_control->connection());

  shill::Daemon daemon(&config, dbus_control);
  daemon.Run();

  return 0;
}
