// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mount.h>
#include <unistd.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/libminijail.h>
#include <chromeos/process.h>
#include <chromeos/syslog_logging.h>

#include "debug_daemon.h"

namespace {
const char* kHelpers[] = {
  NULL,
};

// @brief Enter a VFS namespace.
//
// We don't want anyone other than our descendants to see our tmpfs.
void enter_vfs_namespace() {
  struct minijail* j = minijail_new();
  minijail_namespace_vfs(j);
  minijail_enter(j);
  minijail_destroy(j);
}

// @brief Enter a minijail.
//
// We are already in a vfs namespace so that our tmpfs is only visible to us and
// our descendants, and we don't want to be root. Note that minijail_enter()
// exits the process if it can't succeed.
void enter_sandbox() {
  static const char* kDebugdUser = "debugd";
  static const char* kDebugdGroup = "debugd";
  struct minijail* j = minijail_new();
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
    PLOG(FATAL) << "mount() failed";
}

// @brief Launch all our helper programs.
void launch_helpers() {
  for (int i = 0; kHelpers[i]; ++i) {
    chromeos::ProcessImpl p;
    p.AddArg(kHelpers[i]);
    p.Start();
    p.Release();
  }
}

// @brief Start the debugd DBus interface.
void start() {
  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection conn = DBus::Connection::SystemBus();
  debugd::DebugDaemon debugd(&conn, &dispatcher);
  if (!debugd.Init())
    LOG(FATAL) << "debugd.Init() failed";
  debugd.Run();
  LOG(FATAL) << "debugd.Run() returned";
}
};  // namespace

int __attribute__((visibility("default"))) main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  enter_vfs_namespace();
  make_tmpfs();
  enter_sandbox();
  launch_helpers();
  start();
  return 0;
}
