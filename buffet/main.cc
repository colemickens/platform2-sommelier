// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/command_line.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/syslog_logging.h>

#include "buffet/libbuffet/dbus_constants.h"
#include "buffet/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::DBusServiceDaemon;
using buffet::dbus_constants::kServiceName;
using buffet::dbus_constants::kRootServicePath;

namespace buffet {

class Daemon : public DBusServiceDaemon {
 public:
  Daemon() : DBusServiceDaemon(kServiceName, kRootServicePath) {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    manager_.reset(new buffet::Manager(object_manager_->AsWeakPtr()));
    manager_->RegisterAsync(
        sequencer->GetHandler("Manager.RegisterAsync() failed.", true));
  }

 private:
  std::unique_ptr<buffet::Manager> manager_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace buffet

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);
  buffet::Daemon daemon;
  return daemon.Run();
}
