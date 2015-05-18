// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/command_line.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/syslog_logging.h>

#include "tpm_manager/common/dbus_interface.h"
#include "tpm_manager/server/dbus_service.h"
#include "tpm_manager/server/tpm_manager_service.h"

using chromeos::dbus_utils::AsyncEventSequencer;

class TpmManagerDaemon : public chromeos::DBusServiceDaemon {
 public:
  TpmManagerDaemon()
      : chromeos::DBusServiceDaemon(tpm_manager::kTpmManagerServiceName) {
    tpm_manager_service_.reset(new tpm_manager::TpmManagerService);
    CHECK(tpm_manager_service_->Initialize());
  }

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    dbus_service_.reset(new tpm_manager::DBusService(
        bus_, tpm_manager_service_.get()));
    dbus_service_->Register(sequencer->GetHandler("Register() failed.", true));
  }

 private:
  std::unique_ptr<tpm_manager::TpmManagerInterface> tpm_manager_service_;
  std::unique_ptr<tpm_manager::DBusService> dbus_service_;

  DISALLOW_COPY_AND_ASSIGN(TpmManagerDaemon);
};

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  TpmManagerDaemon daemon;
  return daemon.Run();
}
