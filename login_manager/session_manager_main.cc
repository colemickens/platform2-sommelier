// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
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
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/glib/object.h>

#include "login_manager/child_job.h"
#include "login_manager/file_checker.h"
#include "login_manager/wipe_mitigator.h"
#include "login_manager/session_manager_service.h"
#include "login_manager/system_utils.h"

using std::string;
using std::vector;
using std::wstring;

// Watches a Chrome binary and restarts it when it crashes. Also watches
// window manager binary as well. Actually supports watching several
// processes specified as command line arguments separated with --.
// Also listens over DBus for the commands specified in interface.h.
// Usage:
//   session_manager --uid=1000 --login --
//     /path/to/command1 [arg1 [arg2 [ . . . ] ] ]
//   [-- /path/to/command2 [arg1 [arg2 [ ... ]]]]

namespace switches {

// Name of the flag that contains the path to the file which disables restart of
// managed jobs upon exit or crash if the file is present.
static const char kDisableChromeRestartFile[] = "disable-chrome-restart-file";
// The default path to this file.
static const char kDisableChromeRestartFileDefault[] =
    "/tmp/disable_chrome_restart";

// Name of the flag specifying UID to be set for each managed job before
// starting it.
static const char kUid[] = "uid";

// Name of the flag that determines the path to log file.
static const char kLogFile[] = "log-file";
// The default path to the log file.
static const char kDefaultLogFile[] = "/var/log/session_manager";

// Flag that causes session manager to show the help message and exit.
static const char kHelp[] = "help";
// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] = "\nAvailable Switches: \n"
"  --disable-chrome-restart-file=</path/to/file>\n"
"    Magic file that causes this program to stop restarting the\n"
"    chrome binary and exit. (default: /tmp/disable_chrome_restart)\n"
"  --uid=[number]\n"
"    Numeric uid to transition to prior to execution.\n"
"  --log-file=</path/to/file>\n"
"    Log file to use. (default: /var/log/session_manager)\n"
"  -- /path/to/program [arg1 [arg2 [ . . . ] ] ]\n"
"    Supplies the required program to execute and its arguments.\n"
"    Multiple programs can be executed by delimiting them with addition --\n"
"    as -- foo a b c -- bar d e f\n";

}  // namespace switches

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  string log_file = cl->GetSwitchValueASCII(switches::kLogFile);
  if (log_file.empty())
    log_file.assign(switches::kDefaultLogFile);
  logging::InitLogging(log_file.c_str(),
                       logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);

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
      DLOG(WARNING) << "failed to parse uid, defaulting to none.";
    }
  }

  // Parse jobs to be run with its args.
  vector<string> loose_args = cl->args();
  vector<vector<string> > arg_lists =
      login_manager::SessionManagerService::GetArgLists(loose_args);
  vector<login_manager::ChildJobInterface*> child_jobs;
  for (size_t i = 0; i < arg_lists.size(); ++i) {
    child_jobs.push_back(new login_manager::ChildJob(arg_lists[i]));
    if (uid_set)
      child_jobs.back()->SetDesiredUid(uid);
  }

  ::g_type_init();
  login_manager::SessionManagerService manager(child_jobs);

  string magic_chrome_file =
      cl->GetSwitchValueASCII(switches::kDisableChromeRestartFile);
  if (magic_chrome_file.empty())
    magic_chrome_file.assign(switches::kDisableChromeRestartFileDefault);
  manager.set_file_checker(new login_manager::FileChecker(magic_chrome_file));
  manager.set_mitigator(
      new login_manager::WipeMitigator(new login_manager::SystemUtils()));

  LOG_IF(FATAL, !manager.Initialize()) << "Failed";
  LOG_IF(FATAL, !manager.Register(chromeos::dbus::GetSystemBusConnection()))
    << "Failed";
  LOG_IF(FATAL, !manager.Run()) << "Failed";
  return 0;
}
