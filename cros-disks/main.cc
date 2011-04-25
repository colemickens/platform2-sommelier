// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple daemon to detect, mount, and eject removable storage devices.

#include "cros-disks-server-impl.h"
#include "disk-manager.h"

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/file_util.h>
#include <base/string_util.h>
#include <chromeos/syslog_logging.h>
#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>
#include <gflags/gflags.h>
#include <glib-object.h>
#include <glib.h>
#include <libudev.h>
#include <metrics/metrics_library.h>


using cros_disks::CrosDisksServer;
using cros_disks::DiskManager;

DEFINE_bool(foreground, false, 
            "Don't daemon()ize; run in foreground.");

// Always logs to the syslog and logs to stderr if
// we are running in the foreground.
void SetupLogging() {
  int log_flags = 0;
  log_flags |= chromeos::kLogToSyslog;
  if (FLAGS_foreground) {
    log_flags |= chromeos::kLogToStderr;
  }
  chromeos::InitLog(log_flags);
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
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);

  SetupLogging();

  if(!FLAGS_foreground) {
    LOG(INFO) << "Daemonizing";
    PLOG_IF(FATAL, daemon(0, 0) == 1) << "daemon() failed";
  }

  LOG(INFO) << "Creating a GMainLoop";
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  CHECK(loop) << "Failed to create a GMainLoop";

  LOG(INFO) << "Creating the dbus dispatcher";
  DBus::Glib::BusDispatcher* dispatcher = 
      new(std::nothrow) DBus::Glib::BusDispatcher();
  CHECK(dispatcher) << "Failed to create a dbus-dispatcher";
  DBus::default_dispatcher = dispatcher;
  dispatcher->attach(NULL);

  LOG(INFO) << "creating server";
  DBus::Connection server_conn = DBus::Connection::SystemBus();
  server_conn.request_name("org.chromium.CrosDisks");

  DiskManager manager;
  CrosDisksServer* server = new(std::nothrow) CrosDisksServer(server_conn,
                                                              &manager);
  CHECK(server) << "Failed to create the cros-disks server";

  LOG(INFO) << "Initializing the metrics library";
  MetricsLibrary metrics_lib;
  metrics_lib.Init();

  // Setup a monitor
  g_io_add_watch_full(g_io_channel_unix_new(manager.udev_monitor_fd()),
                      G_PRIORITY_HIGH_IDLE,
                      GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_NVAL),
                      UdevCallback,
                      server,
                      NULL);
  g_main_loop_run(loop);

  LOG(INFO) << "Cleaining up and exiting";
  g_main_loop_unref(loop);
  delete server;
  delete dispatcher;

  return 0;
}
