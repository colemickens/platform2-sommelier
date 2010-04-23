// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <gflags/gflags.h>

#include "modem_manager_server.h"
#include "plugin_manager.h"

DEFINE_string(plugins, NULL,
              "comma-separated list of plugins to load at startup");

DBus::BusDispatcher dispatcher;

void onsignal(int sig) {
  dispatcher.leave();
}

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  signal(SIGTERM, onsignal);
  signal(SIGINT, onsignal);

  DBus::default_dispatcher = &dispatcher;

  DBus::Connection conn = DBus::Connection::SystemBus();
  conn.request_name(ModemManagerServer::SERVER_NAME);

  ModemManagerServer server(conn);

  // Instantiate modem managers for each type of hardware supported
  PluginManager::LoadPlugins(&server, FLAGS_plugins);

  dispatcher.enter();

  return 0;
}
