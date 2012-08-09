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
    "/var/run/disable_chrome_restart";

// Name of the flag specifying UID to be set for each managed job before
// starting it.
static const char kUid[] = "uid";

// Flag that causes session manager to show the help message and exit.
static const char kHelp[] = "help";
// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] = "\nAvailable Switches: \n"
"  --disable-chrome-restart-file=</path/to/file>\n"
"    Magic file that causes this program to stop restarting the\n"
"    chrome binary and exit. (default: /var/run/disable_chrome_restart)\n"
"  --uid=[number]\n"
"    Numeric uid to transition to prior to execution.\n"
"  -- /path/to/program [arg1 [arg2 [ . . . ] ] ]\n"
"    Supplies the required program to execute and its arguments.\n"
"    Multiple programs can be executed by delimiting them with addition --\n"
"    as -- foo a b c -- bar d e f\n";

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
      DLOG(WARNING) << "failed to parse uid, defaulting to none.";
    }
  }

  SystemUtils system;
  // Parse jobs to be run with its args.
  vector<string> loose_args = cl->GetArgs();
  vector<vector<string> > arg_lists =
      SessionManagerService::GetArgLists(loose_args);
  vector<ChildJobInterface*> child_jobs;
  for (size_t i = 0; i < arg_lists.size(); ++i) {
    child_jobs.push_back(new ChildJob(arg_lists[i], &system));
    if (uid_set)
      child_jobs.back()->SetDesiredUid(uid);
  }

  ::g_type_init();
  scoped_refptr<SessionManagerService> manager =
      new SessionManagerService(child_jobs, &system);

  string magic_chrome_file =
      cl->GetSwitchValueASCII(switches::kDisableChromeRestartFile);
  if (magic_chrome_file.empty())
    magic_chrome_file.assign(switches::kDisableChromeRestartFileDefault);
  manager->set_file_checker(new FileChecker(FilePath(magic_chrome_file)));
  manager->set_mitigator(new RegenMitigator(new KeyGenerator(&system),
                                            uid_set,
                                            uid,
                                            manager));
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
