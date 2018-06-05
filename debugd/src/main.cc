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
#include <chromeos/scoped_minijail.h>

#include "debugd/src/debugd_dbus_adaptor.h"

namespace {

// @brief Enter a VFS namespace.
//
// We don't want anyone other than our descendants to see our tmpfs.
void enter_vfs_namespace() {
  ScopedMinijail j(minijail_new());

  // Create a minimalistic mount namespace with just the bare minimum required.
  minijail_namespace_vfs(j.get());
  if (minijail_enter_pivot_root(j.get(), "/var/empty"))
    LOG(FATAL) << "minijail_enter_pivot_root() failed";
  if (minijail_bind(j.get(), "/", "/", 0))
    LOG(FATAL) << "minijail_bind(\"/\") failed";
  if (minijail_mount_with_data(j.get(), "none", "/proc", "proc",
                               MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr)) {
    LOG(FATAL) << "minijail_mount_with_data(\"/proc\") failed";
  }
  if (minijail_bind(j.get(), "/var", "/var", 1))
    LOG(FATAL) << "minijail_bind(\"/var\") failed";

  // Mount /run/dbus to be able to communicate with D-Bus.
  if (minijail_mount_with_data(j.get(), "tmpfs", "/run", "tmpfs",
                               MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr)) {
    LOG(FATAL) << "minijail_mount_with_data(\"/run\") failed";
  }
  if (minijail_bind(j.get(), "/run/dbus", "/run/dbus", 0))
    LOG(FATAL) << "minijail_bind(\"/run/dbus\") failed";

  // Mount /tmp and /run/cups to be able to communicate with CUPS.
  if (minijail_bind(j.get(), "/tmp", "/tmp", 1))
    LOG(FATAL) << "minijail_bind(\"/tmp\") failed";
  // In case we start before cups, make sure the path exists.
  mkdir("/run/cups", 0755);
  if (minijail_bind(j.get(), "/run/cups", "/run/cups", 0))
    LOG(FATAL) << "minijail_bind(\"/run/cups\") failed";

  // Mount /dev to be able to inspect devices.
  if (minijail_mount_with_data(j.get(), "/dev", "/dev", "bind",
                               MS_BIND | MS_REC, nullptr)) {
    LOG(FATAL) << "minijail_mount_with_data(\"/dev\") failed";
  }

  // Mount /sys to access some logs.
  if (minijail_mount_with_data(j.get(), "/sys", "/sys", "bind",
                               MS_BIND | MS_REC, nullptr)) {
    LOG(FATAL) << "minijail_mount_with_data(\"/sys\") failed";
  }

  minijail_enter(j.get());
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
