// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>

#include <base/command_line.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/syslog_logging.h>

#include "peerd/dbus_constants.h"
#include "peerd/manager.h"

using chromeos::DBusServiceDaemon;
using chromeos::dbus_utils::AsyncEventSequencer;
using peerd::Manager;
using peerd::dbus_constants::kManagerServicePath;
using peerd::dbus_constants::kRootServicePath;
using peerd::dbus_constants::kServiceName;

namespace {

static const char kHelpFlag[] = "help";
static const char kLogToStdErrFlag[] = "log_to_stderr";
static const char kHelpMessage[] = "\n"
    "This is the peer discovery service daemon.\n"
    "Usage: peerd [--v=<logging level>]\n"
    "             [--vmodule=<see base/logging.h>]\n"
    "             [--log_to_stderr]";

class Daemon : public DBusServiceDaemon {
 public:
  Daemon() : DBusServiceDaemon(kServiceName, kRootServicePath) {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    manager_.reset(new Manager(object_manager_.get()));
    manager_->RegisterAsync(
        sequencer->GetHandler("Manager.RegisterAsync() failed.", true));
    LOG(INFO) << "peerd starting";
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
  chromeos::InitFlags flags = chromeos::kLogToSyslog;
  if (cl->HasSwitch(kLogToStdErrFlag)) {
    flags = chromeos::kLogToStderr;
  }
  chromeos::InitLog(flags | chromeos::kLogHeader);
  Daemon daemon;
  return daemon.Run();
}
