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
                "The TPM owner password. When specified, additional tests that "
                "require knowledge of the password are executed. When omitted "
                "or empty, such tests are skipped.");
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
      brillo::SecureBlob(FLAGS_owner_password));
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
