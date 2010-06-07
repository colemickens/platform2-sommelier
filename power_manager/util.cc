// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "base/logging.h"

namespace power_manager {

namespace util {

bool LoggedIn() {
  return access("/var/run/state/logged-in", F_OK) == 0;
}

void Launch(const char* cmd) {
  LOG(INFO) << "Launching " << cmd;
  pid_t pid = fork();
  if (pid == 0) {
    // Detach from parent so that powerd doesn't need to wait around for us
    setsid();
    exit(fork() == 0 ? system(cmd) : 0);
  } else if (pid > 0) {
    waitpid(pid, NULL, 0);
  }
}

}  // namespace util

}  // namespace power_manager
