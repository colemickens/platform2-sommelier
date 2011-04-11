// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple daemon to detect, mount, and eject removable storage devices.

#include "cros-disks-server-impl.h"
#include "disk-manager.h"
#include <base/basictypes.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_util.h>
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

// TODO(rtc): gflags string defines require the use of
// -fno-strict-aliasing for some reason. Verify that disabling this check
// is sane.
DEFINE_string(log_dir, "/var/log/cros-disks", "log directory");

void SetupLogging() {
  logging::InitLogging(FLAGS_log_dir.c_str(),
                       logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
}

// This callback will be invoked once udev has data about 
// new, changed, or removed devices.
gboolean UdevCallback(GIOChannel* source, 
                      GIOCondition condition, 
                      gpointer data) {
  DiskManager* mgr = static_cast<DiskManager*>(data);
  mgr->ProcessUdevChanges();
  return true;
}

int main(int argc, char** argv) {
  ::g_type_init();
  g_thread_init(NULL);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if(!FLAGS_foreground) {
    PLOG_IF(FATAL, daemon(0, 0) == 1) << "daemon() failed";
    // SetupLogging();
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
  CrosDisksServer* server = new(std::nothrow) CrosDisksServer(server_conn);
  CHECK(server) << "Failed to create the cros-disks server";

  LOG(INFO) << "Initializing the metrics library";
  MetricsLibrary metrics_lib;
  metrics_lib.Init();


  DiskManager manager;
  manager.EnumerateDisks();

  // Setup a monitor
  g_io_add_watch_full(g_io_channel_unix_new(manager.udev_monitor_fd()),
                      G_PRIORITY_HIGH_IDLE,
                      GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_NVAL),
                      UdevCallback,
                      &manager,
                      NULL);
  g_main_loop_run(loop);

  LOG(INFO) << "Cleaining up and exiting";
  g_main_loop_unref(loop);
  delete server;
  delete dispatcher;

  return 0;
}
