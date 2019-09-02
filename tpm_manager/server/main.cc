//
// Copyright (C) 2015 The Android Open Source Project
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

#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <brillo/syslog_logging.h>
#include <tpm_manager-client/tpm_manager/dbus-constants.h>
#if USE_TPM2
#include <trunks/trunks_factory_impl.h>
#endif

#include "tpm_manager/server/dbus_service.h"
#include "tpm_manager/server/local_data_store.h"
#include "tpm_manager/server/local_data_store_impl.h"
#include "tpm_manager/server/tpm_manager_service.h"

namespace {

constexpr char kWaitForOwnershipTriggerSwitch[] = "wait_for_ownership_trigger";
constexpr char kLogToStderrSwitch[] = "log_to_stderr";
constexpr char kNoPreinitFlagFile[] = "/run/tpm_manager/no_preinit";

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  int flags = brillo::kLogToSyslog;
  if (cl->HasSwitch(kLogToStderrSwitch)) {
    flags |= brillo::kLogToStderr;
  }
  brillo::InitLog(flags);

  tpm_manager::LocalDataStoreImpl local_data_store;
  bool perform_preinit = !base::PathExists(base::FilePath(kNoPreinitFlagFile));

  std::unique_ptr<tpm_manager::TpmManagerService> tpm_manager_service{
      new tpm_manager::TpmManagerService(
          cl->HasSwitch(kWaitForOwnershipTriggerSwitch), perform_preinit,
          &local_data_store)};

  // From now on, the ownership of |tpm_manager_service| is transferred from
  // main function to |ipc_service|.
  tpm_manager::DBusService ipc_service(std::move(tpm_manager_service),
                                       &local_data_store);

  LOG(INFO) << "Starting TPM Manager...";
  return ipc_service.Run();
}
