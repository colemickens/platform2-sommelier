// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/syslog_logging.h>
#include <dbus/bus.h>
#include <sysexits.h>

#include "peerd/manager.h"

using peerd::Manager;

namespace {

static const char kHelpFlag[] = "help";
static const char kHelpMessage[] = "\n"
    "This is the peer discovery service daemon.\n"
    "Usage: peerd [--v=<logging level>] [--vmodule=<see base/logging.h>]\n";

void EnterMainLoop(base::MessageLoopForIO* message_loop,
                   scoped_refptr<dbus::Bus> bus) {
  // TODO(wiley) Initialize global objects here.
  Manager manager(bus);
  // TODO(wiley) Claim DBus service name here.
  LOG(INFO) << "peerd starting";
  message_loop->Run();
}

}  // namespace

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kHelpFlag)) {
    LOG(INFO) << kHelpMessage;
    return EX_USAGE;
  }
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  CHECK(bus->Connect()) << "Failed to connect to DBus.";
  EnterMainLoop(&message_loop, bus);
  bus->ShutdownAndBlock();
  return EX_OK;
}
