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

#include <string>

#include <base/command_line.h>
#include <brillo/syslog_logging.h>
#if defined(USE_TPM2)
#include <trunks/trunks_factory_impl.h>
#endif

#include "tpm_manager/common/tpm_manager_constants.h"
#if defined(USE_BINDER_IPC)
#include "tpm_manager/server/binder_service.h"
#else
#include "tpm_manager/server/dbus_service.h"
#endif
#include "tpm_manager/server/tpm_manager_service.h"

namespace {

constexpr char kWaitForOwnershipTriggerSwitch[] = "wait_for_ownership_trigger";
constexpr char kLogToStderrSwitch[] = "log_to_stderr";

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  int flags = brillo::kLogToSyslog;
  if (cl->HasSwitch(kLogToStderrSwitch)) {
    flags |= brillo::kLogToStderr;
  }
  brillo::InitLog(flags);

  tpm_manager::TpmManagerService tpm_manager_service(
      cl->HasSwitch(kWaitForOwnershipTriggerSwitch));
#if defined(USE_BINDER_IPC)
  tpm_manager::BinderService ipc_service(&tpm_manager_service,
                                         &tpm_manager_service);
#else
  tpm_manager::DBusService ipc_service(&tpm_manager_service,
                                       &tpm_manager_service);
#endif
  CHECK(tpm_manager_service.Initialize()) << "Failed to initialize service.";
  LOG(INFO) << "Starting TPM Manager...";
  return ipc_service.Run();
}
