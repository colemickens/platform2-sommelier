// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple daemon to detect, mount, and eject removable storage devices.

#include <glib.h>
#include <glib-object.h>
#include <libudev.h>

#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/file_util.h>
#include <base/string_util.h>
#include <chromeos/syslog_logging.h>
#include <gflags/gflags.h>
#include <metrics/metrics_library.h>

#include "cros-disks/archive-manager.h"
#include "cros-disks/cros-disks-server-impl.h"
#include "cros-disks/disk-manager.h"
#include "cros-disks/format-manager.h"
#include "cros-disks/platform.h"
#include "cros-disks/power-manager-proxy.h"
#include "cros-disks/session-manager-proxy.h"

using cros_disks::ArchiveManager;
using cros_disks::CrosDisksServer;
using cros_disks::DiskManager;
using cros_disks::FormatManager;
using cros_disks::Platform;
using cros_disks::PowerManagerProxy;
using cros_disks::SessionManagerProxy;

DEFINE_bool(foreground, false,
            "Don't daemon()ize; run in foreground.");
DEFINE_int32(minloglevel, logging::LOG_WARNING,
             "Messages logged at a lower level than "
             "this don't actually get logged anywhere");

static const char kUsageMessage[] = "Chromium OS Disk Daemon";
static const char kArchiveMountRootDirectory[] = "/media/archive";
static const char kDiskMountRootDirectory[] = "/media/removable";
static const char kNonPrivilegedMountUser[] = "chronos";

// Always logs to the syslog and logs to stderr if
// we are running in the foreground.
void SetupLogging() {
  int log_flags = 0;
  log_flags |= chromeos::kLogToSyslog;
  if (FLAGS_foreground) {
    log_flags |= chromeos::kLogToStderr;
  }
  chromeos::InitLog(log_flags);
  logging::SetMinLogLevel(FLAGS_minloglevel);
}

// This callback will be invoked once udev has data about
// new, changed, or removed devices.
gboolean UdevCallback(GIOChannel* source,
                      GIOCondition condition,
                      gpointer data) {
  CrosDisksServer* server = static_cast<CrosDisksServer*>(data);
  server->SignalDeviceChanges();
  // This function should always return true so that the main loop
  // continues to select on udev monitor's file descriptor.
  return true;
}

int main(int argc, char** argv) {
  ::g_type_init();
  g_thread_init(NULL);
  google::SetUsageMessage(kUsageMessage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);

  SetupLogging();

  if (!FLAGS_foreground) {
    LOG(INFO) << "Daemonizing";
    PLOG_IF(FATAL, daemon(0, 0) == 1) << "daemon() failed";
  }

  LOG(INFO) << "Creating a GMainLoop";
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  CHECK(loop) << "Failed to create a GMainLoop";

  LOG(INFO) << "Creating the D-Bus dispatcher";
  DBus::Glib::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  dispatcher.attach(NULL);

  LOG(INFO) << "Creating the cros-disks server";
  DBus::Connection server_conn = DBus::Connection::SystemBus();
  server_conn.request_name("org.chromium.CrosDisks");

  Platform platform;
  CHECK(platform.SetMountUser(kNonPrivilegedMountUser))
      << "'" << kNonPrivilegedMountUser
      << "' is not available for non-privileged mount operations.";

  LOG(INFO) << "Creating the archive manager";
  ArchiveManager archive_manager(kArchiveMountRootDirectory, &platform);
  CHECK(archive_manager.Initialize())
      << "Failed to initialize the archive manager";

  LOG(INFO) << "Creating the disk manager";
  DiskManager disk_manager(kDiskMountRootDirectory, &platform);
  CHECK(disk_manager.Initialize()) << "Failed to initialize the disk manager";

  FormatManager format_manager;
  CrosDisksServer cros_disks_server(server_conn,
                                    &platform,
                                    &archive_manager,
                                    &disk_manager,
                                    &format_manager);

  LOG(INFO) << "Creating a power manager proxy";
  PowerManagerProxy power_manager_proxy(&server_conn, &cros_disks_server);

  LOG(INFO) << "Creating a session manager proxy";
  SessionManagerProxy session_manager_proxy(&server_conn, &cros_disks_server);

  LOG(INFO) << "Initializing the metrics library";
  MetricsLibrary metrics_lib;
  metrics_lib.Init();

  // Setup a monitor
  g_io_add_watch_full(g_io_channel_unix_new(disk_manager.udev_monitor_fd()),
                      G_PRIORITY_HIGH_IDLE,
                      GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_NVAL),
                      UdevCallback,
                      &cros_disks_server,
                      NULL);
  g_main_loop_run(loop);

  LOG(INFO) << "Cleaining up and exiting";
  g_main_loop_unref(loop);

  return 0;
}
