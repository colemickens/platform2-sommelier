// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a small setuid-root program that runs a few commands on behalf of
// the powerd process.

#include <fcntl.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdarg>
#include <cstdlib>
#include <string>

#include <gflags/gflags.h>

#include "base/logging.h"
#include "base/string_number_conversions.h"

DEFINE_string(action, "", "Action to perform.  Must be one of "
              "\"clean_shutdown\", \"lock_vt\", \"reboot\", \"shutdown\", "
              "\"suspend\", and \"unlock_vt\".");
DEFINE_string(shutdown_reason, "", "Optional shutdown reason starting with a "
              "lowercase letter and consisting only of lowercase letters and "
              "dashes.");
DEFINE_uint64(suspend_wakeup_count, 0, "Pass --wakeup_count <INT> to "
              "powerd_suspend for the \"suspend\" action.");
DEFINE_bool(suspend_wakeup_count_valid, false,
            "Should --suspend_wakeup_count be honored?");

// Maximum number of arguments supported for internally-defined commands.
const size_t kMaxArgs = 64;

// Path to device on which VT_UNLOCKSWITCH and VT_LOCKSWITCH ioctls can be made
// to enable or disable VT switching.
const char kConsolePath[] = "/dev/tty0";

// Paths to various binaries that are executed.
const char kInitctlPath[] = "/sbin/initctl";
const char kPowerdSuspendPath[] = "/usr/bin/powerd_suspend";
const char kShutdownPath[] = "/sbin/shutdown";

// Runs a command with the supplied arguments.  The argument list must be
// NULL-terminated.  This method calls exec() without forking, so it will never
// return.
void RunCommand(const char* path, const char* arg, ...) {
  char* argv[kMaxArgs + 1];
  size_t num_args = 1;
  argv[0] = const_cast<char*>(path);

  va_list list;
  va_start(list, arg);
  while (arg) {
    num_args++;
    CHECK(num_args <= kMaxArgs) << "Too many arguments";
    argv[num_args - 1] = const_cast<char*>(arg);
    arg = va_arg(list, char*);
  }
  va_end(list);
  argv[num_args] = NULL;

  // initctl commands appear to fail if the real UID isn't set correctly.
  PCHECK(setuid(0) == 0) << "setuid() failed";
  PCHECK(clearenv() == 0) << "clearenv() failed";
  PCHECK(execv(path, argv) != -1) << "execv() failed";
}

// Locks or unlocks VT switching.  In a perfect world this would live in powerd,
// but these ioctls require the CAP_SYS_TTY_CONFIG capability and setting that
// breaks the setuid() call in RunCommand().  In a slightly less perfect world
// this would live in the powerd_suspend script, but there doesn't seem to be
// any way for a script to lock or unlock switching directly.
void SetVTSwitchingAllowed(bool allowed) {
  int fd = open(kConsolePath, O_WRONLY);
  PCHECK(fd >= 0) << "open(" << kConsolePath << ") failed";
  PCHECK(ioctl(fd, allowed ? VT_UNLOCKSWITCH : VT_LOCKSWITCH) == 0)
      << "ioctl() failed";
  close(fd);
}

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_action == "clean_shutdown") {
    RunCommand(kInitctlPath, "emit", "power-manager-clean-shutdown", NULL);
  } else if (FLAGS_action == "lock_vt") {
    SetVTSwitchingAllowed(false);
  } else if (FLAGS_action == "reboot") {
    RunCommand(kShutdownPath, "-r", "now", NULL);
  } else if (FLAGS_action == "shutdown") {
    std::string reason_arg;
    if (!FLAGS_shutdown_reason.empty()) {
      for (size_t i = 0; i < FLAGS_shutdown_reason.size(); ++i) {
        char ch = FLAGS_shutdown_reason[i];
        CHECK((ch >= 'a' && ch <= 'z') || (i > 0 && ch == '-'))
            << "Invalid shutdown reason";
      }
      reason_arg = "SHUTDOWN_REASON=" + FLAGS_shutdown_reason;
    }
    RunCommand(kInitctlPath, "emit", "--no-wait", "runlevel", "RUNLEVEL=0",
               (reason_arg.empty() ? NULL : reason_arg.c_str()), NULL);
  } else if (FLAGS_action == "suspend") {
    std::string wakeup_flag;
    if (FLAGS_suspend_wakeup_count_valid) {
      wakeup_flag = "--wakeup_count=" +
          base::Uint64ToString(FLAGS_suspend_wakeup_count);
    }
    RunCommand(kPowerdSuspendPath,
               wakeup_flag.empty() ? NULL : wakeup_flag.c_str(), NULL);
  } else if (FLAGS_action == "unlock_vt") {
    SetVTSwitchingAllowed(true);
  } else {
    LOG(ERROR) << "Unknown action \"" << FLAGS_action << "\"";
    return 1;
  }

  return 0;
}
