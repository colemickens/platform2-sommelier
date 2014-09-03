// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple daemon to detect, mount, and eject removable storage devices.

#include <glib.h>
#include <glib-object.h>
#include <glib-unix.h>
#include <libudev.h>

#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>

#include <base/command_line.h>
#include <base/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/syslog_logging.h>

#include "cros-disks/daemon.h"

using cros_disks::Daemon;
using std::string;

namespace {

namespace switches {

// Command line switch to run this daemon in foreground.
const char kForeground[] = "foreground";
// Command line switch to show the help message and exit.
const char kHelp[] = "help";
// Command line switch to set the logging level:
//   0 = LOG(INFO), 1 = LOG(WARNING), 2 = LOG(ERROR)
const char kLogLevel[] = "log-level";
// Help message to show when the --help command line switch is specified.
const char kHelpMessage[] =
    "Chromium OS Disk Daemon\n"
    "\n"
    "Available Switches:\n"
    "  --foreground\n"
    "    Do not daemonize; run in foreground.\n"
    "  --log-level=N\n"
    "    Logging level:\n"
    "      0: LOG(INFO), 1: LOG(WARNING), 2: LOG(ERROR)\n"
    "      -1: VLOG(1), -2: VLOG(2), etc\n"
    "  --help\n"
    "    Show this help.\n"
    "\n";

}  // namespace switches

int GetLogLevel(const string& log_level_value) {
  int log_level = 0;
  if (!base::StringToInt(log_level_value, &log_level)) {
    LOG(WARNING) << "Invalid log level '" << log_level_value << "'";
  } else if (log_level >= logging::LOG_NUM_SEVERITIES) {
    log_level = logging::LOG_NUM_SEVERITIES;
  }
  return log_level;
}

// Always logs to syslog and stderr when running in the foreground.
void SetupLogging(bool foreground, int log_level) {
  int log_flags = chromeos::kLogToSyslog;
  if (foreground)
    log_flags |= chromeos::kLogToStderr;

  chromeos::InitLog(log_flags);
  logging::SetMinLogLevel(log_level);
}

// This callback will be invoked once there is a new device event that
// should be processed by the Daemon::ProcessDeviceEvents().
gboolean DeviceEventCallback(GIOChannel* source,
                             GIOCondition condition,
                             gpointer data) {
  Daemon* daemon = reinterpret_cast<Daemon*>(data);
  daemon->ProcessDeviceEvents();
  // This function should always return true so that the main loop
  // continues to select on device event file descriptor.
  return true;
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

}  // namespace

int main(int argc, char** argv) {
  g_type_init();

  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  bool foreground = cl->HasSwitch(switches::kForeground);
  int log_level = cl->HasSwitch(switches::kLogLevel) ?
      GetLogLevel(cl->GetSwitchValueASCII(switches::kLogLevel)) : 0;

  SetupLogging(foreground, log_level);

  if (!foreground)
    PLOG_IF(FATAL, ::daemon(0, 0) == 1) << "Failed to create daemon";

  LOG(INFO) << "Creating a GMainLoop";
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);
  CHECK(loop) << "Failed to create a GMainLoop";

  // Set up a signal handler for handling SIGINT and SIGTERM.
  g_unix_signal_add(SIGINT, TerminationSignalCallback, loop);
  g_unix_signal_add(SIGTERM, TerminationSignalCallback, loop);

  LOG(INFO) << "Creating the D-Bus dispatcher";
  DBus::Glib::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  dispatcher.attach(nullptr);

  LOG(INFO) << "Creating the cros-disks server";
  DBus::Connection server_conn = DBus::Connection::SystemBus();
  CHECK(server_conn.acquire_name(cros_disks::kCrosDisksServiceName))
      << "Failed to acquire D-Bus name " << cros_disks::kCrosDisksServiceName;

  Daemon daemon(&server_conn);
  daemon.Initialize();

  // Set up a monitor for handling device events.
  GIOChannel* device_event_channel =
      g_io_channel_unix_new(daemon.GetDeviceEventDescriptor());
  g_io_channel_set_close_on_unref(device_event_channel, TRUE);
  CHECK_EQ(G_IO_STATUS_NORMAL,
           g_io_channel_set_encoding(device_event_channel, nullptr, nullptr));
  g_io_add_watch_full(device_event_channel,
                      G_PRIORITY_HIGH_IDLE,
                      GIOCondition(G_IO_IN | G_IO_PRI),  // Ignore errors.
                      DeviceEventCallback,
                      &daemon,
                      nullptr);
  g_main_loop_run(loop);

  LOG(INFO) << "Cleaning up and exiting";
  g_main_loop_unref(loop);

  return 0;
}
