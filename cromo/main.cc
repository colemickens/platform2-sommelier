// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <syslog.h>

#include <dbus-c++/glib-integration.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "cromo_server.h"
#include "plugin_manager.h"

DEFINE_string(plugins, "",
              "comma-separated list of plugins to load at startup");

DBus::Glib::BusDispatcher dispatcher;
GMainLoop *main_loop;

void onsignal(int sig) {
  g_main_loop_quit(main_loop);
}


namespace google {
class LogSinkSyslog : public google::LogSink {
 public:
  LogSinkSyslog() {
    openlog("cromo",
            0,  // Options
            LOG_LOCAL3);  // 5,6,7 are taken
  }

  virtual void send(LogSeverity severity, const char* full_filename,
                    const char* base_filename, int line,
                    const struct ::tm* tm_time,
                    const char* message, size_t message_len) {
    static const int glog_to_syslog[NUM_SEVERITIES] = {
      LOG_INFO, LOG_WARNING, LOG_ERR, LOG_CRIT};
    CHECK(severity < google::NUM_SEVERITIES && severity >= 0);

    syslog(glog_to_syslog[severity],
           "%s:%d %.*s",
           base_filename, line, message_len, message);
  }

  virtual ~LogSinkSyslog() {
    closelog();
  }
};
}  // namespace google

int main(int argc, char* argv[]) {
  google::LogSinkSyslog syslogger;

  google::ParseCommandLineFlags(&argc, &argv, true);
  google::SetCommandLineOptionWithMode("log_dir",
                                       "/tmp",
                                       google::SET_FLAG_IF_DEFAULT);
  google::SetCommandLineOptionWithMode("min_log_level",
                                       "0",
                                       google::SET_FLAG_IF_DEFAULT);
  google::InitGoogleLogging(argv[0]);
  google::AddLogSink(&syslogger);
  google::InstallFailureSignalHandler();

  signal(SIGTERM, onsignal);
  signal(SIGINT, onsignal);

  DBus::default_dispatcher = &dispatcher;
  dispatcher.attach(NULL);

  DBus::Connection conn = DBus::Connection::SystemBus();
  conn.request_name(CromoServer::kServiceName);

  CromoServer server(conn);

  // Instantiate modem handlers for each type of hardware supported
  PluginManager::LoadPlugins(&server, FLAGS_plugins);

  dispatcher.enter();
  main_loop = g_main_loop_new(NULL, false);
  g_main_loop_run(main_loop);

  return 0;
}
