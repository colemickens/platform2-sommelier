// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>
#include <string>

#include <base/command_line.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/minijail/minijail.h>
#include <chromeos/syslog_logging.h>
#include <chromeos/userdb_utils.h>

#include "tpm_manager/common/dbus_interface.h"
#include "tpm_manager/server/dbus_service.h"
#include "tpm_manager/server/local_data_store_impl.h"
#include "tpm_manager/server/tpm_manager_service.h"

#if USE_TPM2
#include "tpm_manager/server/tpm2_initializer_impl.h"
#include "tpm_manager/server/tpm2_status_impl.h"
#else
#include "tpm_manager/server/tpm_initializer_impl.h"
#include "tpm_manager/server/tpm_status_impl.h"
#endif

using chromeos::dbus_utils::AsyncEventSequencer;

namespace {

const char kWaitForOwnershipTriggerSwitch[] = "wait_for_ownership_trigger";

class TpmManagerDaemon : public chromeos::DBusServiceDaemon {
 public:
  TpmManagerDaemon()
      : chromeos::DBusServiceDaemon(tpm_manager::kTpmManagerServiceName) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    local_data_store_.reset(new tpm_manager::LocalDataStoreImpl());
#if USE_TPM2
    tpm_status_.reset(new tpm_manager::Tpm2StatusImpl);
    tpm_initializer_.reset(new tpm_manager::Tpm2InitializerImpl(
        local_data_store_.get(),
        tpm_status_.get()));
#else
    tpm_status_.reset(new tpm_manager::TpmStatusImpl);
    tpm_initializer_.reset(new tpm_manager::TpmInitializerImpl(
        local_data_store_.get(),
        tpm_status_.get()));
#endif
    tpm_manager_service_.reset(new tpm_manager::TpmManagerService(
        command_line->HasSwitch(kWaitForOwnershipTriggerSwitch),
        local_data_store_.get(),
        tpm_status_.get(),
        tpm_initializer_.get()));
  }

 protected:
  int OnInit() override {
    int result = chromeos::DBusServiceDaemon::OnInit();
    if (result != EX_OK) {
      LOG(ERROR) << "Error starting tpm_manager dbus daemon.";
      return result;
    }
    CHECK(tpm_manager_service_->Initialize());
    return EX_OK;
  }

  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    dbus_service_.reset(new tpm_manager::DBusService(
        bus_, tpm_manager_service_.get()));
    dbus_service_->Register(sequencer->GetHandler("Register() failed.", true));
  }

 private:
  std::unique_ptr<tpm_manager::LocalDataStore> local_data_store_;
  std::unique_ptr<tpm_manager::TpmStatus> tpm_status_;
  std::unique_ptr<tpm_manager::TpmInitializer> tpm_initializer_;
  std::unique_ptr<tpm_manager::TpmManagerInterface> tpm_manager_service_;
  std::unique_ptr<tpm_manager::DBusService> dbus_service_;

  DISALLOW_COPY_AND_ASSIGN(TpmManagerDaemon);
};

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  TpmManagerDaemon daemon;
  LOG(INFO) << "TpmManager Daemon Started.";
  return daemon.Run();
}
