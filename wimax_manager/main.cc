// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>
#include <glib-object.h>
#include <glib-unix.h>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>
#include <gflags/gflags.h>

DEFINE_bool(foreground, false,
            "Don't daemon()ize; run in foreground.");
DEFINE_int32(minloglevel, logging::LOG_WARNING,
             "Messages logged at a lower level than "
             "this don't actually get logged anywhere");

namespace {

const char kUsageMessage[] = "Chromium OS WiMAX Manager";

// Always logs to syslog and stderr when running in the foreground.
void SetupLogging() {
  int log_flags = chromeos::kLogToSyslog;
  if (FLAGS_foreground)
    log_flags |= chromeos::kLogToStderr;

  chromeos::InitLog(log_flags);
  logging::SetMinLogLevel(FLAGS_minloglevel);
}

// This callback will be inovked when this process receives SIGINT or SIGTERM.
gboolean TerminationSignalCallback(gpointer data) {
  GMainLoop *loop = reinterpret_cast<GMainLoop *>(data);
  g_main_loop_quit(loop);
  // This function can return false to remove this signal handler as we are
  // quitting the main loop anyway.
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  g_type_init();
  g_thread_init(NULL);
  google::SetUsageMessage(kUsageMessage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);
  SetupLogging();

  if (!FLAGS_foreground) {
    LOG(INFO) << "Daemonizing WiMAX manager";
    PLOG_IF(FATAL, ::daemon(0, 0) == 1) << "Failed to create daemon";
  }

  GMainLoop *loop = g_main_loop_new(g_main_context_default(), FALSE);
  CHECK(loop) << "Failed to create GLib main loop";

  // Set up a signal handler for handling SIGINT and SIGTERM.
  g_unix_signal_add(SIGINT, TerminationSignalCallback, loop);
  g_unix_signal_add(SIGTERM, TerminationSignalCallback, loop);

  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  return 0;
}
