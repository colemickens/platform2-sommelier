// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/async_event_sequencer.h>
#include <chromeos/exported_object_manager.h>
#include <chromeos/syslog_logging.h>
#include <dbus/bus.h>
#include <sysexits.h>

#include "peerd/dbus_constants.h"
#include "peerd/manager.h"

using dbus::ObjectPath;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;
using peerd::Manager;
using peerd::dbus_constants::kManagerServicePath;

namespace {

static const char kHelpFlag[] = "help";
static const char kHelpMessage[] = "\n"
    "This is the peer discovery service daemon.\n"
    "Usage: peerd [--v=<logging level>] [--vmodule=<see base/logging.h>]\n";

void TakeServiceOwnership(scoped_refptr<dbus::Bus> bus, bool success) {
  // Success should always be true since we've said that failures are
  // fatal.
  CHECK(success) << "Init of one or more objects has failed.";
  CHECK(bus->RequestOwnershipAndBlock(peerd::dbus_constants::kServiceName,
                                      dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << peerd::dbus_constants::kServiceName;
}

void EnterMainLoop(base::MessageLoopForIO* message_loop,
                   scoped_refptr<dbus::Bus> bus) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  ExportedObjectManager object_manager(bus, ObjectPath(kManagerServicePath));
  Manager manager(&object_manager);
  manager.RegisterAsync(sequencer->GetHandler("Manager.Init() failed.", true));
  sequencer->OnAllTasksCompletedCall({base::Bind(&TakeServiceOwnership, bus)});
  // Release our handle on the sequencer so that it gets deleted after
  // both callbacks return.
  sequencer = nullptr;
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
