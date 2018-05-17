// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/syslog_logging.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/libminijail.h>

#include "debugd/src/debugd_dbus_adaptor.h"

namespace {

// @brief Enter a VFS namespace.
//
// We don't want anyone other than our descendants to see our tmpfs.
void enter_vfs_namespace() {
  struct minijail* j = minijail_new();
  minijail_namespace_vfs(j);
  minijail_enter(j);
  minijail_destroy(j);
}

class Daemon : public brillo::DBusServiceDaemon {
 public:
  Daemon() : DBusServiceDaemon(debugd::kDebugdServiceName) {}

 protected:
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override {
    adaptor_.reset(new debugd::DebugdDBusAdaptor(bus_));
    adaptor_->RegisterAsync(sequencer->GetHandler(
        "RegisterAsync() failed.", true));
  }

 private:
  std::unique_ptr<debugd::DebugdDBusAdaptor> adaptor_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);
  enter_vfs_namespace();
  Daemon().Run();
  return 0;
}
