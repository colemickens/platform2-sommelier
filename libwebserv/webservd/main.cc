// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/command_line.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/syslog_logging.h>

#include "libwebserv/webservd/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;

namespace {

const char kServiceName[] = "org.chromium.WebServer";
const char kRootServicePath[] = "/org/chromium/WebServer";

class Daemon : public chromeos::DBusServiceDaemon {
 public:
  Daemon() : DBusServiceDaemon(kServiceName, kRootServicePath) {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    manager_.reset(new webservd::Manager{object_manager_.get()});
    manager_->RegisterAsync(
        sequencer->GetHandler("Manager.RegisterAsync() failed.", true));
    LOG(INFO) << "webservd starting...";
  }

 private:
  std::unique_ptr<webservd::Manager> manager_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog |
                    chromeos::kLogToStderr |
                    chromeos::kLogHeader);
  Daemon daemon;
  return daemon.Run();
}
