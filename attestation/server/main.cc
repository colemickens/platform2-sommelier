//
// Copyright (C) 2014 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <sysexits.h>

#include <memory>
#include <string>

#include <base/command_line.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/minijail/minijail.h>
#include <chromeos/syslog_logging.h>
#include <chromeos/userdb_utils.h>

#include "attestation/common/dbus_interface.h"
#include "attestation/server/attestation_service.h"
#include "attestation/server/dbus_service.h"

#include <chromeos/libminijail.h>

namespace {

const uid_t kRootUID = 0;
const char kAttestationUser[] = "attestation";
const char kAttestationGroup[] = "attestation";
const char kAttestationSeccompPath[] =
    "/usr/share/policy/attestationd-seccomp.policy";

void InitMinijailSandbox() {
  uid_t attestation_uid;
  gid_t attestation_gid;
  CHECK(chromeos::userdb::GetUserInfo(kAttestationUser,
                                      &attestation_uid,
                                      &attestation_gid))
      << "Error getting attestation uid and gid.";
  CHECK_EQ(getuid(), kRootUID) << "AttestationDaemon not initialized as root.";
  chromeos::Minijail* minijail = chromeos::Minijail::GetInstance();
  struct minijail* jail = minijail->New();

  minijail->DropRoot(jail, kAttestationUser, kAttestationGroup);
  minijail->UseSeccompFilter(jail, kAttestationSeccompPath);
  minijail->Enter(jail);
  minijail->Destroy(jail);
  CHECK_EQ(getuid(), attestation_uid)
      << "AttestationDaemon was not able to drop to attestation user.";
  CHECK_EQ(getgid(), attestation_gid)
      << "AttestationDaemon was not able to drop to attestation group.";
}

}  // namespace

using chromeos::dbus_utils::AsyncEventSequencer;

class AttestationDaemon : public chromeos::DBusServiceDaemon {
 public:
  AttestationDaemon()
      : chromeos::DBusServiceDaemon(attestation::kAttestationServiceName) {
    attestation_service_.reset(new attestation::AttestationService);
    // Move initialize call down to OnInit
    CHECK(attestation_service_->Initialize());
  }

 protected:
  int OnInit() override {
    int result = chromeos::DBusServiceDaemon::OnInit();
    if (result != EX_OK) {
      LOG(ERROR) << "Error starting attestation dbus daemon.";
      return result;
    }
    return EX_OK;
  }

  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    dbus_service_.reset(new attestation::DBusService(
        bus_,
        attestation_service_.get()));
    dbus_service_->Register(sequencer->GetHandler("Register() failed.", true));
  }

 private:
  std::unique_ptr<attestation::AttestationInterface> attestation_service_;
  std::unique_ptr<attestation::DBusService> dbus_service_;

  DISALLOW_COPY_AND_ASSIGN(AttestationDaemon);
};

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  AttestationDaemon daemon;
  LOG(INFO) << "Attestation Daemon Started.";
  InitMinijailSandbox();
  return daemon.Run();
}
