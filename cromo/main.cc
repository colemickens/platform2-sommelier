// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <syslog.h>
#include <sys/signalfd.h>

#include <dbus-c++/glib-integration.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "cromo_server.h"
#include "plugin_manager.h"

DEFINE_string(plugins, "",
              "comma-separated list of plugins to load at startup");

DBus::Glib::BusDispatcher dispatcher;
GMainLoop *main_loop;
static CromoServer *server;

static const int kExitMaxTries = 10;
static int kExitTries = 0;

// This function is run on a timer by exit_main_loop(). It calls all of the
// exit-ok hooks to see if they are all ready for the program to exit; it also
// keeps track of tries so that we time out appropriately if one of the devices
// isn't disconnecting properly.
static gboolean test_for_exit(void *arg) {
  int *tries = static_cast<int*>(arg);
  if ((*tries)++ < kExitMaxTries && !server->exit_ok_hooks().Run())
    // Event needs to be scheduled again.
    return TRUE;
  g_main_loop_quit(main_loop);
  // We're done here; exit the program.
  return FALSE;
}

// This function starts exiting the main loop. We run all the pre-exit hooks,
// then keep testing every second to see if all the exit hooks think it's okay
// to exit.
static void exit_main_loop(void) {
  server->start_exit_hooks().Run();
  if (server->exit_ok_hooks().Run()) {
    g_main_loop_quit(main_loop);
    return;
  }
  g_timeout_add_seconds(1, test_for_exit, &kExitTries);
}

static gboolean do_signal(void *arg) {
  int sig = reinterpret_cast<int>(arg);
  LOG(INFO) << "Signal: " << sig;

  if (sig == SIGTERM) {
    exit_main_loop();
  }

  return FALSE;
}

static void *handle_signals(void *arg) {
  sigset_t sigs;
  siginfo_t info;
  info.si_signo = 0;
  sigemptyset(&sigs);
  sigaddset(&sigs, SIGTERM);
  sigaddset(&sigs, SIGINT);
  LOG(INFO) << "waiting for signals";
  while (info.si_signo != SIGTERM) {
    sigwaitinfo(&sigs, &info);
    g_idle_add(do_signal, reinterpret_cast<void*>(info.si_signo));
  }
  return NULL;
}

static gboolean setup_signals(void* arg) {
  pthread_t thr;
  pthread_create(&thr, NULL, handle_signals, NULL);
  return FALSE;
}

static void block_signals(void) {
  sigset_t sigs;
  int res;

  sigemptyset(&sigs);
  sigaddset(&sigs, SIGTERM);
  sigaddset(&sigs, SIGINT);
  res = pthread_sigmask(SIG_BLOCK, &sigs, NULL);
  LOG(INFO) << "signals blocked";
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

  block_signals();

  DBus::default_dispatcher = &dispatcher;
  dispatcher.attach(NULL);

  DBus::Connection conn = DBus::Connection::SystemBus();
  conn.request_name(CromoServer::kServiceName);

  server = new CromoServer(conn);

  // Instantiate modem handlers for each type of hardware supported
  PluginManager::LoadPlugins(server, FLAGS_plugins);

  dispatcher.enter();
  g_thread_init(NULL);
  main_loop = g_main_loop_new(NULL, false);
  g_idle_add(setup_signals, NULL);
  g_main_loop_run(main_loop);

  return 0;
}
