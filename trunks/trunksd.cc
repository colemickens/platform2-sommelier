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

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/threading/thread.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/libminijail.h>
#include <chromeos/minijail/minijail.h>
#include <chromeos/syslog_logging.h>
#include <chromeos/userdb_utils.h>

#include "trunks/background_command_transceiver.h"
#include "trunks/dbus_interface.h"
#include "trunks/resource_manager.h"
#include "trunks/tpm_handle.h"
#include "trunks/tpm_simulator_handle.h"
#include "trunks/trunks_factory_impl.h"
#include "trunks/trunks_ftdi_spi.h"
#include "trunks/trunks_service.h"

using chromeos::dbus_utils::AsyncEventSequencer;

namespace {

const uid_t kRootUID = 0;
const char kTrunksUser[] = "trunks";
const char kTrunksGroup[] = "trunks";
const char kTrunksSeccompPath[] = "/usr/share/policy/trunksd-seccomp.policy";
const char kBackgroundThreadName[] = "trunksd_background_thread";

void InitMinijailSandbox() {
  uid_t trunks_uid;
  gid_t trunks_gid;
  CHECK(chromeos::userdb::GetUserInfo(kTrunksUser,
                                      &trunks_uid,
                                      &trunks_gid))
      << "Error getting trunks uid and gid.";
  CHECK_EQ(getuid(), kRootUID) << "Trunks Daemon not initialized as root.";
  chromeos::Minijail* minijail = chromeos::Minijail::GetInstance();
  struct minijail* jail = minijail->New();
  minijail->DropRoot(jail, kTrunksUser, kTrunksGroup);
  minijail->UseSeccompFilter(jail, kTrunksSeccompPath);
  minijail->Enter(jail);
  minijail->Destroy(jail);
  CHECK_EQ(getuid(), trunks_uid)
      << "TrunksDaemon was not able to drop to trunks user.";
  CHECK_EQ(getgid(), trunks_gid)
      << "TrunksDaemon was not able to drop to trunks group.";
}

}  // namespace

class TrunksDaemon : public chromeos::DBusServiceDaemon {
 public:
  explicit TrunksDaemon(trunks::CommandTransceiver* transceiver) :
      chromeos::DBusServiceDaemon(trunks::kTrunksServiceName) {
    transceiver_.reset(transceiver);
    background_thread_.reset(new base::Thread(kBackgroundThreadName));
    CHECK(background_thread_->Start());
    // Chain together command transceivers:
    //   [IPC] --> TrunksService --> BackgroundCommandTransceiver -->
    //       ResourceManager --> TpmHandle --> [TPM]
    factory_.reset(new trunks::TrunksFactoryImpl(transceiver_.get()));
    resource_manager_.reset(new trunks::ResourceManager(
        *factory_,
        transceiver_.get()));
    background_thread_->message_loop_proxy()->PostNonNestableTask(
        FROM_HERE,
        base::Bind(&trunks::ResourceManager::Initialize,
        base::Unretained(resource_manager_.get())));
    background_transceiver_.reset(
        new trunks::BackgroundCommandTransceiver(
            resource_manager_.get(),
            background_thread_->message_loop_proxy()));
  }

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    trunks_service_.reset(new trunks::TrunksService(
        bus_,
        background_transceiver_.get()));
    trunks_service_->Register(
        sequencer->GetHandler("Register() failed.", true));
  }


 private:
  std::unique_ptr<trunks::TrunksService> trunks_service_;
  std::unique_ptr<trunks::CommandTransceiver> transceiver_;
  // Thread for executing TPM comands.
  std::unique_ptr<base::Thread> background_thread_;
  std::unique_ptr<trunks::TrunksFactory> factory_;
  std::unique_ptr<trunks::ResourceManager> resource_manager_;
  std::unique_ptr<trunks::CommandTransceiver> background_transceiver_;

  DISALLOW_COPY_AND_ASSIGN(TrunksDaemon);
};

int main(int argc, char **argv) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  base::CommandLine *cl = base::CommandLine::ForCurrentProcess();
  trunks::CommandTransceiver *transceiver;
  if (cl->HasSwitch("ftdi")) {
    transceiver = new trunks::TrunksFtdiSpi();
  } else if (cl->HasSwitch("simulator")) {
    transceiver = new trunks::TpmSimulatorHandle();
  } else {
    transceiver = new trunks::TpmHandle();
  }
  CHECK(transceiver->Init()) << "Error initializing transceiver";
  TrunksDaemon daemon(transceiver);
  InitMinijailSandbox();
  LOG(INFO) << "Trunks Service Started";
  return daemon.Run();
}
