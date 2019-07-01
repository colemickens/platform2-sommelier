// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include <brillo/syslog_logging.h>

#include "tpm_softclear_utils/tpm.h"

int main(int argc, char* argv[]) {
  // All logs go to the system log file.
  int flags = brillo::kLogToSyslog;
  brillo::InitLog(flags);

  std::unique_ptr<tpm_softclear_utils::Tpm>
      tpm(tpm_softclear_utils::Tpm::Create());

  base::Optional<std::vector<uint8_t>> auth_value = tpm->GetAuthForOwnerReset();
  if (!auth_value) {
    LOG(ERROR) << "Unable to soft-clear the TPM: failed to get the auth value.";
    return -1;
  }

  if (!tpm->SoftClearOwner(*auth_value)) {
    LOG(ERROR) << "Unable to soft-clear the TPM.";
    return -1;
  }

  LOG(INFO) << "TPM is soft-cleared.";
  return 0;
}
