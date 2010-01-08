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

#include "login_manager/child_job.h"
#include "login_manager/file_checker.h"
#include "login_manager/ipc_channel.h"
#include "login_manager/session_manager.h"

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

static const char kPipe[] = "pipe";
static const char kPipeDefault[] = "/tmp/session_manager_pipe";

static const char kHelp[] = "help";
static const char kHelpMessage[] = "\nAvailable Switches: \n"
"  --disable-chrome-restart-file=</path/to/file>\n"
"    Magic file that causes this program to stop restarting the\n"
"    chrome binary and exit. (default: /tmp/disable_chrome_restart)\n"
"  --uid=[number]\n"
"    Numeric uid to transition to prior to execution.\n"
"  --login\n"
"    session_manager will append --login-manager to the child's command line.\n"
"  --pipe=[/path/to/pipe]\n"
"    FIFO over which session_manager can communicate with the child.\n"
"    Only used if --login is specified. (default: /tmp/session_manager_pipe)\n"
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
  login_manager::IpcReadChannel* reader = NULL;
  std::string pipe_name;

  // If there's no |kPipe|, or it doesn't specify a value, use the default.
  if (!cl->HasSwitch(switches::kPipe) ||
      cl->GetSwitchValueASCII(switches::kPipe).empty()) {
    pipe_name.assign(switches::kPipeDefault);
  } else {
    pipe_name.assign(cl->GetSwitchValueASCII(switches::kPipe));
  }
  if (set_uid)
    reader = new login_manager::IpcReadChannel(pipe_name, uid);
  else
    reader = new login_manager::IpcReadChannel(pipe_name);

  login_manager::SetUidExecJob* child_job =
      new login_manager::SetUidExecJob(cl, add_login_flag, pipe_name);
  if (set_uid)
    child_job->set_uid(uid);

  login_manager::SessionManager manager(new login_manager::FileChecker,
                                        reader,
                                        child_job,
                                        add_login_flag);
  manager.LoopChrome(magic_chrome_file.c_str());
}
