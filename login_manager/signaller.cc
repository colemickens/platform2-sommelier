// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Not intended for use on the system, this is a handy helper binary for
// manually testing the session_manager.  It may find its way into
// autotest-based tests of this component.

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include "login_manager/ipc_channel.h"

namespace switches {
static const char kExitSad[] = "exit-sad";
static const char kSuicide[] = "suicide";
static const char kSessionPipe[] = "session-manager-pipe";
}

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  CommandLine *cl = CommandLine::ForCurrentProcess();
  LOG(INFO) << "running as " << getuid() << ", " << argv[argc-1];

  if (cl->HasSwitch(switches::kSessionPipe)) {
    login_manager::IpcWriteChannel writer(
        cl->GetSwitchValueASCII(switches::kSessionPipe));
    writer.init();
    writer.send(login_manager::EMIT_LOGIN);
    writer.send(login_manager::START_SESSION);
    writer.shutdown();
  }

  pid_t pid = fork();
  if (pid ==0) {
    LOG(INFO) << "PGID is " << getpgid(getpid());
    sleep(3);
    exit(47);
  }

  int exit_val = 0;
  if (cl->HasSwitch(switches::kExitSad)) {
    exit_val = 1;
  }
  sleep(1);
  if (cl->HasSwitch(switches::kSuicide)) {
    int* foo = 0;
    *foo = 1;
  }
  exit(exit_val);
}
