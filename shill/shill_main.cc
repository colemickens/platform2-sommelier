// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <chromeos/syslog_logging.h>

#include "shill/dbus_control.h"
#include "shill/shill_config.h"
#include "shill/shill_daemon.h"

using std::string;
using std::vector;

namespace switches {

// Don't daemon()ize; run in foreground.
static const char kForeground[] = "foreground";
// Directory to read confguration settings.
static const char kConfigDir[] = "config-dir";
// Directory to read default configuration settings (Read Only).
static const char kDefaultConfigDir[] = "default-config-dir";
// Don't attempt to manage these devices.
static const char kDeviceBlackList[] = "device-black-list";
// Flag that causes shill to show the help message and exit.
static const char kHelp[] = "help";
// LOG() level. 0 = INFO, 1 = WARNING, 2 = ERROR.
static const char kLogLevel[] = "log-level";
// Use the same directories flimflam uses for global, user profiles..
static const char kUseFlimflamProfiles[] = "use-flimflam-profiles";

// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] = "\n"
    "Available Switches: \n"
    "  --foreground\n"
    "    Don\'t daemon()ize; run in foreground.\n"
    "  --config-dir\n"
    "    Directory to read confguration settings.\n"
    "  --default-config-dir\n"
    "    Directory to read default configuration settings (Read Only).\n"
    "  --device-black-list=device1,device2\n"
    "    Do not manage devices named device1 or device2\n"
    "  --log-level=N\n"
    "    LOG() level. 0 = INFO, 1 = WARNING, 2 = ERROR.\n"
    "  --use-flimflam-profiles\n"
    "    Use the same directories flimflam uses for global, user profiles.\n"
    "  --v=N\n"
    "    Enables VLOG(N) and below.\n"
    "  --vmodule=\"*file_pattern*=1,certain_file.cc=2\".\n"
    "    Enable VLOG() at different levels in different files/modules.\n";
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

void DeleteDBusControl(void* param) {
  VLOG(2) << __func__;
  shill::DBusControl* dbus_control =
      reinterpret_cast<shill::DBusControl*>(param);
  delete dbus_control;
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
  if (cl->HasSwitch(switches::kLogLevel)) {
    std::string log_level = cl->GetSwitchValueASCII(switches::kLogLevel);
    int level = 0;
    if (base::StringToInt(log_level, &level) &&
        level >= 0 && level < logging::LOG_NUM_SEVERITIES) {
      logging::SetMinLogLevel(level);
    } else {
      LOG(WARNING) << "Bad log level: " << log_level;
    }
  }

  FilePath config_dir(cl->GetSwitchValueASCII(switches::kConfigDir));
  FilePath default_config_dir(
      !cl->HasSwitch(switches::kDefaultConfigDir) ?
      shill::Config::kShillDefaultPrefsDir :
      cl->GetSwitchValueASCII(switches::kDefaultConfigDir));

  shill::Config config; /* (config_dir, default_config_dir) */
  if (cl->HasSwitch(switches::kUseFlimflamProfiles))
    config.UseFlimflamStorageDirs();

  // TODO(pstew): This should be chosen based on config
  // Make sure we delete the DBusControl object AFTER the LazyInstances
  // since some LazyInstances destructors rely on D-Bus being around.
  shill::DBusControl* dbus_control = new shill::DBusControl();
  exit_manager.RegisterCallback(DeleteDBusControl, dbus_control);
  dbus_control->Init();

  shill::Daemon daemon(&config, dbus_control);

  if (cl->HasSwitch(switches::kDeviceBlackList)) {
    vector<string> device_list;
    base::SplitString(cl->GetSwitchValueASCII(switches::kDeviceBlackList),
                      ',', &device_list);

    vector<string>::iterator i;
    for (i = device_list.begin(); i != device_list.end(); ++i) {
      daemon.AddDeviceToBlackList(*i);
    }
  }
  daemon.Run();

  return 0;
}
