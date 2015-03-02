// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <sysexits.h>

#include <base/command_line.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/syslog_logging.h>

#include "leaderd/manager.h"

using chromeos::DBusServiceDaemon;
using chromeos::dbus_utils::AsyncEventSequencer;
using leaderd::Manager;

namespace {

// TODO(wiley) Remove these in favor of values from the adaptor.
const char kServiceName[] = "org.chromium.leaderd";
const char kRootServicePath[] = "/org/chromium/leaderd";

const char kHelpFlag[] = "help";
const char kHelpMessage[] =
    "\n"
    "This daemon allows groups of devices to elect a leader device.\n"
    "Usage: leaderd [--v=<logging level>]\n"
    "               [--vmodule=<see base/logging.h>]\n";

class Daemon : public DBusServiceDaemon {
 public:
  Daemon() : DBusServiceDaemon{kServiceName, kRootServicePath} {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    manager_.reset(new Manager{object_manager_.get()});
    manager_->RegisterAsync(
        sequencer->GetHandler("Manager.RegisterAsync() failed.", true));
    LOG(INFO) << "leaderd starting";
  }

 private:
  std::unique_ptr<Manager> manager_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kHelpFlag)) {
    LOG(INFO) << kHelpMessage;
    return EX_USAGE;
  }
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);
  Daemon daemon;
  return daemon.Run();
}
