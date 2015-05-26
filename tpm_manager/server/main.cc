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
#include "tpm_manager/server/tpm_manager_service.h"

using chromeos::dbus_utils::AsyncEventSequencer;

namespace {

const uid_t kRootUID = 0;
const char kTpmManagerUser[] = "tpm_manager";
const char kTpmManagerGroup[] = "tpm_manager";
const char kTpmManagerSeccompPath[] =
    "/usr/share/policy/tpm_managerd-seccomp.policy";

void InitMinijailSandbox() {
  uid_t tpm_manager_uid;
  gid_t tpm_manager_gid;
  CHECK(chromeos::userdb::GetUserInfo(kTpmManagerUser,
                                      &tpm_manager_uid,
                                      &tpm_manager_gid))
      << "Error getting tpm_manager uid and gid.";
  CHECK_EQ(getuid(), kRootUID) << "TpmManagerDaemon not initialized as root.";
  chromeos::Minijail* minijail = chromeos::Minijail::GetInstance();
  struct minijail* jail = minijail->New();
  minijail->DropRoot(jail, kTpmManagerUser, kTpmManagerGroup);
  minijail->UseSeccompFilter(jail, kTpmManagerSeccompPath);
  minijail->Enter(jail);
  minijail->Destroy(jail);
  CHECK_EQ(getuid(), tpm_manager_uid)
      << "TpmManagerDaemon was not able to drop to tpm_manager user.";
  CHECK_EQ(getgid(), tpm_manager_gid)
      << "TpmManagerDaemon was not able to drop to tpm_manager group.";
}

}  // namespace

class TpmManagerDaemon : public chromeos::DBusServiceDaemon {
 public:
  TpmManagerDaemon()
      : chromeos::DBusServiceDaemon(tpm_manager::kTpmManagerServiceName) {
    tpm_manager_service_.reset(new tpm_manager::TpmManagerService);
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
  std::unique_ptr<tpm_manager::TpmManagerInterface> tpm_manager_service_;
  std::unique_ptr<tpm_manager::DBusService> dbus_service_;

  DISALLOW_COPY_AND_ASSIGN(TpmManagerDaemon);
};

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  TpmManagerDaemon daemon;
  LOG(INFO) << "TpmManager Daemon Started";
  InitMinijailSandbox();
  return daemon.Run();
}
