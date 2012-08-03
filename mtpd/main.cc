// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple daemon to detect and access PTP/MTP devices.

#include <glib.h>
#include <glib-object.h>
#include <glib-unix.h>

#include <dbus-c++/glib-integration.h>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/syslog_logging.h>
#include <gflags/gflags.h>

DEFINE_bool(foreground, false,
            "Don't daemon()ize; run in foreground.");
DEFINE_int32(minloglevel, logging::LOG_WARNING,
             "Messages logged at a lower level than "
             "this don't actually get logged anywhere");

static const char kUsageMessage[] = "Chromium OS MTP Daemon";

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

// This callback will be inovked when this process receives SIGINT or SIGTERM.
gboolean TerminationSignalCallback(gpointer data) {
  LOG(INFO) << "Received a signal to terminate the daemon";
  GMainLoop* loop = reinterpret_cast<GMainLoop*>(data);
  g_main_loop_quit(loop);
  // This function can return false to remove this signal handler as we are
  // quitting the main loop anyway.
  return false;
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
    PLOG_IF(FATAL, ::daemon(0, 0) == 1) << "daemon() failed";
  }

  LOG(INFO) << "Creating a GMainLoop";
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  CHECK(loop) << "Failed to create a GMainLoop";

  // Set up a signal handler for handling SIGINT and SIGTERM.
  g_unix_signal_add(SIGINT, TerminationSignalCallback, loop);
  g_unix_signal_add(SIGTERM, TerminationSignalCallback, loop);

  LOG(INFO) << "Creating the D-Bus dispatcher";
  DBus::Glib::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  dispatcher.attach(NULL);

  g_main_loop_run(loop);

  LOG(INFO) << "Cleaning up and exiting";
  g_main_loop_unref(loop);

  return 0;
}
