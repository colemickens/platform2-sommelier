// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Standalone tool that executes tests on a live TPM.

#include <cstdlib>

#include <base/at_exit.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <brillo/secure_blob.h>
#include <brillo/syslog_logging.h>
#include <openssl/evp.h>

#include "cryptohome/tpm.h"
#include "cryptohome/tpm_live_test.h"

int main(int argc, char** argv) {
  DEFINE_string(owner_password, "",
                "The TPM owner password. If the device is equipped with TPM "
                "1.2, then when this flag is specified some additional tests "
                "that require knowledge of the password are executed. When "
                "omitted or empty, such tests are skipped. This flag has no "
                "effect on TPM 2.0 systems.");
  DEFINE_bool(tpm2_use_system_owner_password, "",
              "Whether the TPM 2.0 owner password which is available to the "
              "system should be used (for example, this password is displayed "
              "by the \"tpm-manager dump_status\" command). If the device is "
              "equipped with TPM 2.0, then when this flag is specified some "
              "additional tests that require availability of the password are "
              "executed; note that these tests will fail if the password turns "
              "out to be missing. This flag has no effect on TPM 1.2 systems.");
  brillo::FlagHelper::Init(argc, argv,
                           "Executes cryptohome tests on a live TPM.\nNOTE: "
                           "the TPM must be available and owned.");
  brillo::InitLog(brillo::kLogToStderr);
  base::AtExitManager exit_manager;
  OpenSSL_add_all_algorithms();
  LOG(INFO) << "Running TPM live tests.";

  // Set up the Tpm singleton state, assuming that the preconditions for running
  // this tool are satisfied.
  cryptohome::Tpm* const tpm = cryptohome::Tpm::GetSingleton();
  if (tpm->GetVersion() != cryptohome::Tpm::TPM_2_0) {
    tpm->SetIsEnabled(true);
    tpm->SetIsOwned(true);
  }

  const bool success = cryptohome::TpmLiveTest().RunLiveTests(
      brillo::SecureBlob(FLAGS_owner_password),
      FLAGS_tpm2_use_system_owner_password);
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
