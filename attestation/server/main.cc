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
#include <unistd.h>

#include <cstdlib>
#include <memory>
#include <string>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/dbus/async_event_sequencer.h>
#include <brillo/minijail/minijail.h>
#include <brillo/secure_blob.h>
#include <brillo/syslog_logging.h>
#include <brillo/userdb_utils.h>

#include "attestation/server/attestation_service.h"
#include "attestation/server/dbus_service.h"
#include "attestation-client/attestation/dbus-constants.h"

#include <chromeos/libminijail.h>

namespace {

const uid_t kRootUID = 0;
const char kAttestationUser[] = "attestation";
const char kAttestationGroup[] = "attestation";
const char kAttestationSeccompPath[] =
    "/usr/share/policy/attestationd-seccomp.policy";

namespace env {
static const char kAttestationBasedEnrollmentDataFile[] = "ABE_DATA_FILE";
}  // namespace env

// Returns the contents of the attestation-based enrollment data file.
std::string ReadAbeDataFileContents() {
  std::string data;

  const char* abe_data_file =
      std::getenv(env::kAttestationBasedEnrollmentDataFile);
  if (!abe_data_file) {
    return data;
  }

  base::FilePath file_path(abe_data_file);
  if (!base::ReadFileToString(file_path, &data)) {
    LOG(FATAL) << "Could not read attestation-based enterprise enrollment data"
                  " in: "
               << file_path.value();
  }

  return data;
}

bool GetAttestationEnrollmentData(const std::string& abe_data_hex,
                                         brillo::SecureBlob* abe_data) {
  abe_data->clear();
  if (abe_data_hex.empty()) return true;  // no data is ok.
  // The data must be a valid 32-byte hex string.
  return brillo::SecureBlob::HexStringToSecureBlob(abe_data_hex, abe_data) &&
         abe_data->size() == 32;
}

void InitMinijailSandbox() {
  uid_t attestation_uid;
  gid_t attestation_gid;
  CHECK(brillo::userdb::GetUserInfo(kAttestationUser, &attestation_uid,
                                    &attestation_gid))
      << "Error getting attestation uid and gid.";
  CHECK_EQ(getuid(), kRootUID) << "AttestationDaemon not initialized as root.";
  brillo::Minijail* minijail = brillo::Minijail::GetInstance();
  struct minijail* jail = minijail->New();

  minijail_log_seccomp_filter_failures(jail);
  minijail->DropRoot(jail, kAttestationUser, kAttestationGroup);
  minijail_inherit_usergroups(jail);
  minijail->UseSeccompFilter(jail, kAttestationSeccompPath);
  minijail->Enter(jail);
  minijail->Destroy(jail);
  CHECK_EQ(getuid(), attestation_uid)
      << "AttestationDaemon was not able to drop to attestation user.";
  CHECK_EQ(getgid(), attestation_gid)
      << "AttestationDaemon was not able to drop to attestation group.";
}

}  // namespace

using brillo::dbus_utils::AsyncEventSequencer;

class AttestationDaemon : public brillo::DBusServiceDaemon {
 public:
  explicit AttestationDaemon(brillo::SecureBlob abe_data)
      : brillo::DBusServiceDaemon(attestation::kAttestationServiceName),
        abe_data_(std::move(abe_data)),
        attestation_service_(&abe_data_) {}

 protected:
  int OnInit() override {
    int result = brillo::DBusServiceDaemon::OnInit();
    if (result != EX_OK) {
      LOG(ERROR) << "Error starting attestation dbus daemon.";
      return result;
    }
    attestation_service_.Initialize();
    return EX_OK;
  }

  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    dbus_service_.reset(
        new attestation::DBusService(bus_, &attestation_service_));
    dbus_service_->Register(sequencer->GetHandler("Register() failed.", true));
  }

 private:
  brillo::SecureBlob abe_data_;
  attestation::AttestationService attestation_service_;
  std::unique_ptr<attestation::DBusService> dbus_service_;

  DISALLOW_COPY_AND_ASSIGN(AttestationDaemon);
};

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  int flags = brillo::kLogToSyslog;
  if (cl->HasSwitch("log_to_stderr")) {
    flags |= brillo::kLogToStderr;
  }
  brillo::InitLog(flags);
  // read whole abe_data_file before we init minijail.
  std::string abe_data_hex = ReadAbeDataFileContents();
  brillo::SecureBlob abe_data;
  if (!GetAttestationEnrollmentData(abe_data_hex, &abe_data)) {
    LOG(ERROR) << "Invalid attestation-based enterprise enrollment data.";
  }
  PLOG_IF(FATAL, daemon(0, 0) == -1) << "Failed to daemonize";
  AttestationDaemon daemon(abe_data);
  LOG(INFO) << "Attestation Daemon Started.";
  InitMinijailSandbox();
  return daemon.Run();
}
