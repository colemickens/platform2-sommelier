// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <string>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>
#include <dbus-c++/eventloop-integration.h>

#include "shill/shims/environment.h"
#include "shill/shims/task_proxy.h"

using shill::shims::Environment;
using shill::shims::TaskProxy;
using std::map;
using std::string;

int main(int argc, char **argv) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);

  Environment *environment = Environment::GetInstance();
  string service, path, reason;
  // TODO(petkov): When switching shill to use this shim, rename the variables
  // and maybe use shared kConsts. Also, get rid of CONNMAN_INTERFACE.
  if (!environment->GetVariable("CONNMAN_BUSNAME", &service) ||
      !environment->GetVariable("CONNMAN_PATH", &path) ||
      !environment->GetVariable("script_type", &reason)) {
    LOG(ERROR) << "Environment variables not available.";
    return EXIT_FAILURE;
  }

  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection connection(DBus::Connection::SystemBus());
  TaskProxy proxy(&connection, path, service);
  map<string, string> env = environment->AsMap();
  proxy.Notify(reason, env);
  return EXIT_SUCCESS;
}
