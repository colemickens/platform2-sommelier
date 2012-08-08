// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <glib.h>
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

#include "shill/dbus_control.h"
#include "shill/memory_log.h"
#include "shill/scope_logger.h"
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
// Technologies to enable for portal check at startup.
static const char kPortalList[] = "portal-list";
// Flag to specify specific profiles to be pushed.
static const char kPushProfiles[] = "push";
// Flag that causes shill to show the help message and exit.
static const char kHelp[] = "help";
// Logging level:
//   0 = LOG(INFO), 1 = LOG(WARNING), 2 = LOG(ERROR),
//   -1 = SLOG(..., 1), -2 = SLOG(..., 2), etc.
static const char kLogLevel[] = "log-level";
// Scopes to enable for SLOG()-based logging.
static const char kLogScopes[] = "log-scopes";
// Use the same directories flimflam uses (profiles, run dir...)
static const char kUseFlimflamDirs[] = "use-flimflam-dirs";

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
    "    Logging level:\n"
    "      0 = LOG(INFO), 1 = LOG(WARNING), 2 = LOG(ERROR),\n"
    "      -1 = SLOG(..., 1), -2 = SLOG(..., 2), etc.\n"
    "  --log-scopes=\"*scope1+scope2\".\n"
    "    Scopes to enable for SLOG()-based logging.\n"
    "  --portal-list=technology1,technology2\n"
    "    Specify technologies to perform portal detection on at startup.\n"
    "  --push=profile1,profile2\n"
    "    Specify profiles to push on startup.\n"
    "  --use-flimflam-dirs\n"
    "    Use the same directories flimflam uses (profiles, run dir...).\n";
}  // namespace switches

namespace {

const char *kLoggerCommand = "/usr/bin/logger";

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
  shill::MemoryLog::InstallLogInterceptor();

  if (!foreground) {
    vector<char *> logger_command_line;
    int logger_stdin_fd;
    logger_command_line.push_back(const_cast<char *>(kLoggerCommand));
    logger_command_line.push_back(const_cast<char *>("--priority"));
    logger_command_line.push_back(const_cast<char *>("daemon.err"));
    logger_command_line.push_back(const_cast<char *>("--tag"));
    logger_command_line.push_back(daemon_name);
    logger_command_line.push_back(NULL);
    if (!g_spawn_async_with_pipes(NULL,
                                  logger_command_line.data(),
                                  NULL,
                                  G_SPAWN_STDERR_TO_DEV_NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  &logger_stdin_fd,
                                  NULL,
                                  NULL,
                                  NULL)) {
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

void DeleteDBusControl(void* param) {
  SLOG(DBus, 2) << __func__;
  shill::DBusControl* dbus_control =
      reinterpret_cast<shill::DBusControl*>(param);
  delete dbus_control;
}

gboolean ExitSigHandler(gpointer data) {
  shill::Daemon* daemon = reinterpret_cast<shill::Daemon*>(data);
  daemon->Quit();
  return TRUE;
}


int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  const int nochdir = 0, noclose = 0;
  if (!cl->HasSwitch(switches::kForeground))
    PLOG_IF(FATAL, daemon(nochdir, noclose) == -1 ) << "Failed to daemonize";

  SetupLogging(cl->HasSwitch(switches::kForeground), argv[0]);
  if (cl->HasSwitch(switches::kLogLevel)) {
    string log_level = cl->GetSwitchValueASCII(switches::kLogLevel);
    int level = 0;
    if (base::StringToInt(log_level, &level) &&
        level < logging::LOG_NUM_SEVERITIES) {
      logging::SetMinLogLevel(level);
      // Like VLOG, SLOG uses negative verbose level.
      shill::ScopeLogger::GetInstance()->set_verbose_level(-level);
    } else {
      LOG(WARNING) << "Bad log level: " << log_level;
    }
  }

  if (cl->HasSwitch(switches::kLogScopes)) {
    string log_scopes = cl->GetSwitchValueASCII(switches::kLogScopes);
    shill::ScopeLogger::GetInstance()->EnableScopesByName(log_scopes);
  }

  FilePath config_dir(cl->GetSwitchValueASCII(switches::kConfigDir));
  FilePath default_config_dir(
      !cl->HasSwitch(switches::kDefaultConfigDir) ?
      shill::Config::kShillDefaultPrefsDir :
      cl->GetSwitchValueASCII(switches::kDefaultConfigDir));

  shill::Config config; /* (config_dir, default_config_dir) */
  if (cl->HasSwitch(switches::kUseFlimflamDirs))
    config.UseFlimflamDirs();

  // TODO(pstew): This should be chosen based on config
  // Make sure we delete the DBusControl object AFTER the LazyInstances
  // since some LazyInstances destructors rely on D-Bus being around.
  shill::DBusControl* dbus_control = new shill::DBusControl();
  exit_manager.RegisterCallback(DeleteDBusControl, dbus_control);
  dbus_control->Init();

  shill::Daemon daemon(&config, dbus_control);

  if (cl->HasSwitch(switches::kPushProfiles)) {
    vector<string> profile_list;
    base::SplitString(cl->GetSwitchValueASCII(switches::kPushProfiles),
                      ',', &profile_list);
    daemon.SetStartupProfiles(profile_list);
  }

  if (cl->HasSwitch(switches::kDeviceBlackList)) {
    vector<string> device_list;
    base::SplitString(cl->GetSwitchValueASCII(switches::kDeviceBlackList),
                      ',', &device_list);

    vector<string>::iterator i;
    for (i = device_list.begin(); i != device_list.end(); ++i) {
      daemon.AddDeviceToBlackList(*i);
    }
  }

  if (cl->HasSwitch(switches::kPortalList)) {
    daemon.SetStartupPortalList(cl->GetSwitchValueASCII(switches::kPortalList));
  }

  g_unix_signal_add(SIGINT, ExitSigHandler, &daemon);
  g_unix_signal_add(SIGTERM, ExitSigHandler, &daemon);

  daemon.Run();

  return 0;
}
