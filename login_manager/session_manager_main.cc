// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/glib/object.h>

#include "login_manager/child_job.h"
#include "login_manager/file_checker.h"
#include "login_manager/session_manager_service.h"

// Watches a Chrome binary and restarts it when it crashes.
// Can also create and watch a named pipe and respond to IPCs defined
// in ipc_messages.h
// Usage:
//   session_manager --uid=1000 --login --pipe=/path/to/pipe --
//     /path/to/command [arg1 [arg2 [ . . . ] ] ]

namespace switches {
static const char kDisableChromeRestartFile[] = "disable-chrome-restart-file";
static const char kDisableChromeRestartFileDefault[] =
    "/tmp/disable_chrome_restart";

static const char kUid[] = "uid";

static const char kLogin[] = "login";

static const char kHelp[] = "help";
static const char kHelpMessage[] = "\nAvailable Switches: \n"
"  --disable-chrome-restart-file=</path/to/file>\n"
"    Magic file that causes this program to stop restarting the\n"
"    chrome binary and exit. (default: /tmp/disable_chrome_restart)\n"
"  --uid=[number]\n"
"    Numeric uid to transition to prior to execution.\n"
"  --login\n"
"    session_manager will append --login-manager to the child's command line.\n"
"  -- /path/to/program [arg1 [arg2 [ . . . ] ] ]\n"
"    Supplies the required program to execute and its arguments.\n";
}  // namespace switches

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  CommandLine *cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    exit(0);
  }

  uid_t uid = 0;
  bool set_uid = false;
  std::string uid_string = cl->GetSwitchValueASCII(switches::kUid);
  if (!uid_string.empty()) {
    errno = 0;
    uid = static_cast<uid_t>(strtol(uid_string.c_str(), NULL, 0));
    if (errno == 0)
      set_uid = true;
    else
      DLOG(WARNING) << "failed to parse uid, defaulting to none.";
  }

  std::string magic_chrome_file =
      cl->GetSwitchValueASCII(switches::kDisableChromeRestartFile);
  if (magic_chrome_file.empty())
    magic_chrome_file.assign(switches::kDisableChromeRestartFileDefault);

  bool add_login_flag = cl->HasSwitch(switches::kLogin);

  login_manager::SetUidExecJob* child_job =
      new login_manager::SetUidExecJob(cl,
                                       new login_manager::FileChecker(
                                           magic_chrome_file.c_str()),
                                       add_login_flag);
  if (set_uid)
    child_job->set_uid(uid);

  ::g_type_init();
  login_manager::SessionManagerService manager(child_job,
                                               false);  // FOR NOW.

  LOG_IF(FATAL, !manager.Initialize()) << "Failed";
  LOG_IF(FATAL, !manager.Register(chromeos::dbus::GetSystemBusConnection()))
    << "Failed";
  LOG_IF(FATAL, !manager.Run()) << "Failed";
  return 0;
}
