// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <syslog.h>
#include <unistd.h>

#include <chromeos/libminijail.h>

#include "debugdaemon.h"

const char *kHelpers[] = {
  NULL,
};

void die(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsyslog(LOG_ERR, fmt, ap);
  va_end(ap);
  exit(1);
}

// @brief Enter a minijail.
//
// We need to be in a vfs namespace so that our tmpfs is only visible to us and
// our descendants, and we don't want to be root. Note that minijail_enter()
// exits the process if it can't succeed.
void enter_sandbox() {
  static const char *kDebugdUser = "debugd";
  static const char *kDebugdGroup = "debugd";
  struct minijail *j = minijail_new();
  minijail_namespace_vfs(j);
  minijail_change_user(j, kDebugdUser);
  minijail_change_group(j, kDebugdGroup);
  minijail_enter(j);
  minijail_destroy(j);
}

// @brief Sets up a tmpfs visible to this program and its descendants.
//
// The created tmpfs is mounted at /debugd.
void make_tmpfs() {
  int r = mount("none", "/debugd", "tmpfs", MS_NODEV | MS_NOSUID | MS_NOEXEC,
                NULL);
  if (r < 0)
    die("mount() failed: %s", strerror(errno));
}

// @brief Launch a single helper program.
//
// Helper programs are launched with no arguments.
void launch_one_helper(const char *progname) {
  // execve() fails to declare the pointers inside its argument array as const,
  // so we have to cast away constness here, even though argv does not touch
  // them.
  char *const argv[] = { const_cast<char *>(progname), NULL };
  int r = fork();
  if (r < 0)
    die("forking helper %s failed: %s", progname, strerror(errno));
  if (r > 0) {
    syslog(LOG_NOTICE, "forked helper %d for %s", r, progname);
    return;
  }
  r = execve(progname, argv, environ);
  syslog(LOG_ERR, "execing helper %s failed: %s", progname, strerror(errno));
  exit(r);
}

// @brief Launch all our helper programs.
void launch_helpers() {
  for (int i = 0; kHelpers[i]; i++) {
    launch_one_helper(kHelpers[i]);
  }
}

void start_dbus() {
  // TODO(ellyjones): Implement this
}

int __attribute__((visibility("default"))) main(int argc, char *argv[]) {
  enter_sandbox();
  make_tmpfs();
  launch_helpers();
  start_dbus();
  return 0;
}
