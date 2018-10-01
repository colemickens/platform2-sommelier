// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/syslog_logging.h>
#include <dbus/oobe_config/dbus-constants.h>

#include "oobe_config/oobe_config.h"
#include "oobe_config/oobe_config_restore_service.h"

using brillo::dbus_utils::AsyncEventSequencer;
using brillo::dbus_utils::DBusObject;

namespace oobe_config {

namespace {

void InitLog() {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);
  logging::SetLogItems(true /* enable_process_id */,
                       true /* enable_thread_id */, true /* enable_timestamp */,
                       true /* enable_tickcount */);
}

}  // namespace

class OobeConfigRestoreDaemon : public brillo::DBusServiceDaemon {
 public:
  explicit OobeConfigRestoreDaemon(bool allow_unencrypted)
      : DBusServiceDaemon(kOobeConfigRestoreServiceName),
        allow_unencrypted_(allow_unencrypted) {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    auto dbus_object = std::make_unique<DBusObject>(
        nullptr, bus_,
        org::chromium::OobeConfigRestoreAdaptor::GetObjectPath());

    service_ = std::make_unique<OobeConfigRestoreService>(
        std::move(dbus_object), allow_unencrypted_);
    service_->RegisterAsync(sequencer->GetHandler(
        "OobeConfigRestoreService.RegisterAsync() failed.", true));
  }

  void OnShutdown(int* return_code) override {
    DBusServiceDaemon::OnShutdown(return_code);
    service_.reset();
  }

 private:
  std::unique_ptr<OobeConfigRestoreService> service_;
  bool allow_unencrypted_;
  DISALLOW_COPY_AND_ASSIGN(OobeConfigRestoreDaemon);
};

// Runs OobeConfigRestoreDaemon.
int RunDaemon(bool allow_unencrypted) {
  OobeConfigRestoreDaemon daemon(allow_unencrypted);

  LOG(INFO) << "Starting oobe_config_restore daemon";
  int res = daemon.Run();

  LOG(INFO) << "oobe_config_restore stopping with exit code " << res;
  return res;
}

}  // namespace oobe_config

// Execute the first stage of the restore process itself immediately (without
// waiting for Chrome to initiate it). Use only for testing.
constexpr char kTestUnencrypted[] = "test-unencrypted";
// Starts the service using unencrypted rollback data. Use only for testing.
constexpr char kAllowUnencrypted[] = "allow-unencrypted";

int main(int argc, char* argv[]) {
  oobe_config::InitLog();

  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kTestUnencrypted)) {
    return oobe_config::OobeConfig().UnencryptedRollbackRestore();
  } else if (cl->HasSwitch(kAllowUnencrypted)) {
    return oobe_config::RunDaemon(/*allow_unencrypted=*/true);
  } else {
    return oobe_config::RunDaemon(/*allow_unencrypted=*/false);
  }
}
