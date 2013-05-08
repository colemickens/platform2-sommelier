// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/time.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/glib/object.h>
#include <chromeos/syslog_logging.h>

#include "login_manager/child_job.h"
#include "login_manager/file_checker.h"
#include "login_manager/regen_mitigator.h"
#include "login_manager/session_manager_service.h"
#include "login_manager/system_utils.h"

using std::string;
using std::vector;
using std::wstring;

// Watches a Chrome binary and restarts it when it crashes. Also watches
// window manager binary as well. Actually supports watching several
// processes specified as command line arguments separated with --.
// Also listens over DBus for the commands specified in dbus_glib_shim.h.
// Usage:
//   session_manager --uid=1000 -- /path/to/command1 [arg1 [arg2 [ . . . ] ] ]

namespace switches {

// Name of the flag that contains the path to the file which disables restart of
// managed jobs upon exit or crash if the file is present.
static const char kDisableChromeRestartFile[] = "disable-chrome-restart-file";
// The default path to this file.
static const char kDisableChromeRestartFileDefault[] =
    "/var/run/disable_chrome_restart";

// Name of the flag specifying UID to be set for each managed job before
// starting it.
static const char kUid[] = "uid";

// Name of flag specifying the time (in s) to wait for children to exit
// gracefully before killing them with a SIGABRT.
static const char kKillTimeout[] = "kill-timeout";
static const int kKillTimeoutDefault = 3;

// Name of the flag specifying whether we should kill and restart chrome
// if we detect that it has hung.
static const char kEnableHangDetection[] = "enable-hang-detection";
static const uint kHangDetectionIntervalDefaultSeconds = 60;

// Name of the flag indicating the session_manager should enable support
// for simultaneous active sessions.
static const char kMultiProfile[] = "multi-profiles";

// Flag that causes session manager to show the help message and exit.
static const char kHelp[] = "help";
// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] = "\nAvailable Switches: \n"
"  --disable-chrome-restart-file=</path/to/file>\n"
"    Magic file that causes this program to stop restarting the\n"
"    chrome binary and exit. (default: /var/run/disable_chrome_restart)\n"
"  --uid=[number]\n"
"    Numeric uid to transition to prior to execution.\n"
"  --kill-timeout=[number in seconds]\n"
"    Number of seconds to wait for children to exit gracefully before\n"
"    killing them with a SIGABRT.\n"
"  --enable-hang-detection[=number in seconds]\n"
"    Ping the browser over DBus periodically to determine if it's alive.\n"
"    Optionally accepts a period value in seconds.  Default is 60.\n"
"    If it fails to respond, SIGABRT and restart it."
"  -- /path/to/program [arg1 [arg2 [ . . . ] ] ]\n"
    "    Supplies the required program to execute and its arguments.\n";

}  // namespace switches

using login_manager::ChildJob;
using login_manager::ChildJobInterface;
using login_manager::FileChecker;
using login_manager::KeyGenerator;
using login_manager::RegenMitigator;
using login_manager::SessionManagerService;
using login_manager::SystemUtils;

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  // Parse UID if it's present, -1 means no UID should be set.
  bool uid_set = false;
  uid_t uid = 0;
  if (cl->HasSwitch(switches::kUid)) {
    string uid_flag = cl->GetSwitchValueASCII(switches::kUid);
    int uid_value = 0;
    if (base::StringToInt(uid_flag, &uid_value)) {
      uid = static_cast<uid_t>(uid_value);
      uid_set = true;
    } else {
      DLOG(WARNING) << "Failed to parse uid, defaulting to none.";
    }
  }

  // Parse kill timeout if it's present.
  int kill_timeout = switches::kKillTimeoutDefault;
  if (cl->HasSwitch(switches::kKillTimeout)) {
    string timeout_flag = cl->GetSwitchValueASCII(switches::kKillTimeout);
    int from_flag = 0;
    if (base::StringToInt(timeout_flag, &from_flag)) {
      kill_timeout = from_flag;
    } else {
      DLOG(WARNING) << "Failed to parse kill timeout, defaulting to "
                    << kill_timeout;
    }
  }

  // Parse hang detection interval if it's present.
  uint hang_detection_interval = switches::kHangDetectionIntervalDefaultSeconds;
  if (cl->HasSwitch(switches::kEnableHangDetection)) {
    string flag = cl->GetSwitchValueASCII(switches::kEnableHangDetection);
    uint from_flag = 0;
    if (base::StringToUint(flag, &from_flag)) {
      hang_detection_interval = from_flag;
    } else {
      DLOG(WARNING) << "Failed to parse hang detection interval, defaulting to "
                    << hang_detection_interval;
    }
  }

  // Check for simultaneous active session support.
  bool support_multi_profile = cl->HasSwitch(switches::kMultiProfile);

  SystemUtils system;
  // We only support a single job with args, so grab all loose args
  vector<string> arg_list = SessionManagerService::GetArgList(cl->GetArgs());

  scoped_ptr<ChildJobInterface> browser_job(new ChildJob(arg_list,
                                                         support_multi_profile,
                                                         &system));
  if (uid_set)
    browser_job->SetDesiredUid(uid);

  ::g_type_init();
  scoped_refptr<SessionManagerService> manager =
      new SessionManagerService(
          browser_job.Pass(),
          kill_timeout,
          cl->HasSwitch(switches::kEnableHangDetection),
          base::TimeDelta::FromSeconds(hang_detection_interval),
          &system);

  string magic_chrome_file =
      cl->GetSwitchValueASCII(switches::kDisableChromeRestartFile);
  if (magic_chrome_file.empty())
    magic_chrome_file.assign(switches::kDisableChromeRestartFileDefault);
  manager->set_file_checker(new FileChecker(FilePath(magic_chrome_file)));

  if (uid_set)
    manager->set_uid(uid);

  LOG_IF(FATAL, !manager->Initialize()) << "Failed";
  LOG_IF(FATAL, !manager->Register(chromeos::dbus::GetSystemBusConnection()))
    << "Failed";
  LOG_IF(FATAL, !manager->Run()) << "Failed";

  LOG_IF(WARNING, manager->exit_code() != SessionManagerService::SUCCESS)
      << "session_manager exiting with code " << manager->exit_code();
  return manager->exit_code();
}
