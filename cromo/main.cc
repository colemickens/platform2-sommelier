// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cromo_server.h"

#include <signal.h>
#include <stdio.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/syslog_logging.h>
#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>

#include "carrier.h"
#include "plugin_manager.h"
#include "sandbox.h"
#include "syslog_helper.h"

#ifndef VCSID
#define VCSID "<not set>"
#endif

static const char *kDBusInterface = "org.freedesktop.DBus";
static const char *kDBusNameOwnerChanged = "NameOwnerChanged";

DBus::Glib::BusDispatcher dispatcher;
GMainLoop *main_loop;
static CromoServer *server;

static const int kExitMaxTries = 10;
static int kExitTries = 0;

namespace switches {

// Comma-separated list of plugins to load at startup
static const char kPlugins[] = "plugins";
// Flag that causes shill to show the help message and exit.
static const char kHelp[] = "help";

// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] = "\n"
    "Available Switches: \n"
    "  --plugins\n"
    "    comma-separated list of plugins to load at startup\n";
}  // namespace switches

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
  long sig = reinterpret_cast<long>(arg);
  LOG(INFO) << "Signal: " << sig;

  if (sig == SIGTERM || sig == SIGINT) {
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
  while (info.si_signo != SIGTERM && info.si_signo != SIGINT) {
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

  sigemptyset(&sigs);
  sigaddset(&sigs, SIGTERM);
  sigaddset(&sigs, SIGINT);
  pthread_sigmask(SIG_BLOCK, &sigs, NULL);
}

class MessageHandler : public DBus::Callback_Base<bool, const DBus::Message&> {
  public:
    MessageHandler(CromoServer *srv) : srv_(srv) { }

    bool NameOwnerChanged(const DBus::Message& param) const {
      DBus::MessageIter iter;
      const char *name;
      const char *oldowner;
      const char *newowner;
      iter = param.reader();
      name = iter.get_string();
      iter++;
      oldowner = iter.get_string();
      iter++;
      newowner = iter.get_string();
      if (name && strcmp(name, power_manager::kPowerManagerInterface) == 0) {
        if (strlen(oldowner) && !strlen(newowner)) {
          srv_->PowerDaemonDown();
        } else if (!strlen(oldowner) && strlen(newowner)) {
          srv_->PowerDaemonUp();
        }
      }
      return true;
    }

    bool PowerStateChanged(const DBus::Message& param) const {
      DBus::MessageIter iter;
      const char *new_power_state;
      iter = param.reader();
      new_power_state = iter.get_string();
      srv_->PowerStateChanged(new_power_state ? new_power_state : "");
      return true;
    }

    bool SuspendDelay(const DBus::Message& param) const {
      unsigned int seqnum;
      DBus::MessageIter iter;
      iter = param.reader();
      seqnum = iter.get_uint32();
      srv_->SuspendDelay(seqnum);
      return true;
    }

    bool call(const DBus::Message& param) const {
      if (param.is_signal(kDBusInterface, kDBusNameOwnerChanged)) {
        return NameOwnerChanged(param);
      }
      if (param.is_signal(power_manager::kPowerManagerInterface,
                          power_manager::kPowerStateChangedSignal)) {
        return PowerStateChanged(param);
      }
      if (param.is_signal(power_manager::kPowerManagerInterface,
                          power_manager::kSuspendDelay)) {
        return SuspendDelay(param);
      }
      return false;
    }
  private:
    CromoServer *srv_;
};

// Always logs to the syslog and stderr.
void SetupLogging(void) {
  int log_flags = 0;
  log_flags |= chromeos::kLogToSyslog;
  log_flags |= chromeos::kLogToStderr;
  log_flags |= chromeos::kLogHeader;
  chromeos::InitLog(log_flags);
}

int main(int argc, char* argv[]) {
  // Drop privs right away for now.
  // TODO(ellyjones): once we do more serious sandboxing, this will need to be
  // broken into two parts, one to be done pre-plugin load and one to be done
  // post-plugin load -- or we can just do the whole thing post-plugin load.
  Sandbox::Enter();

  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  SysLogHelperInit();
  SetupLogging();

  block_signals();

  DBus::default_dispatcher = &dispatcher;
  dispatcher.attach(NULL);

  DBus::Connection conn = DBus::Connection::SystemBus();
  conn.request_name(CromoServer::kServiceName);

  server = new CromoServer(conn);
  std::string match =
      base::StringPrintf("type='signal',interface='%s',member='%s'",
                         kDBusInterface, kDBusNameOwnerChanged);
  conn.add_match(match.c_str());
  match = base::StringPrintf("type='signal',interface='%s',member='%s'",
                             power_manager::kPowerManagerInterface,
                             power_manager::kPowerStateChangedSignal);
  conn.add_match(match.c_str());
  match = base::StringPrintf("type='signal',interface='%s',member='%s'",
                             power_manager::kPowerManagerInterface,
                             power_manager::kSuspendDelay);
  conn.add_match(match.c_str());
  DBus::MessageSlot mslot;
  mslot = new MessageHandler(server);
  if (!conn.add_filter(mslot)) {
    LOG(ERROR) << "Can't add filter";
  } else {
    LOG(INFO) << "Registered filter.";
  }

  // Add carriers before plugins so that they can be overidden
  AddBaselineCarriers(server);

  // Instantiate modem handlers for each type of hardware supported
  std::string plugins = cl->GetSwitchValueASCII(switches::kPlugins);
  PluginManager::LoadPlugins(server, plugins);

  server->CheckForPowerDaemon();

  dispatcher.enter();
  g_thread_init(NULL);
  main_loop = g_main_loop_new(NULL, false);
  g_idle_add(setup_signals, NULL);
  g_main_loop_run(main_loop);

  PluginManager::UnloadPlugins(false);
  LOG(INFO) << "Exit";
  return 0;
}
