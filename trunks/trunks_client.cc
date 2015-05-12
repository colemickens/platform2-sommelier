// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// trunks_client is a command line tool that supports various TPM operations. It
// does not provide direct access to the trunksd D-Bus interface.

#include <stdio.h>
#include <string>

#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>

#include "trunks/error_codes.h"
#include "trunks/hmac_session.h"
#include "trunks/password_authorization_delegate.h"
#include "trunks/policy_session.h"
#include "trunks/scoped_key_handle.h"
#include "trunks/tpm_state.h"
#include "trunks/tpm_utility.h"
#include "trunks/trunks_client_test.h"
#include "trunks/trunks_factory_impl.h"

namespace {

void PrintUsage() {
  puts("Options:");
  puts("  --help - Prints this message.");
  puts("  --status - Prints TPM status information.");
  puts("  --startup - Performs startup and self-tests.");
  puts("  --clear - Clears the TPM. Use before initializing the TPM.");
  puts("  --init_tpm - Initializes a TPM as CrOS firmware does.");
  puts("  --allocate_pcr - Configures PCR 0-15 under the SHA256 bank.");
  puts("  --own - Takes ownership of the TPM with the provided password.");
  puts("  --regression_test - Runs some basic regression tests. If");
  puts("                      owner_password is supplied, it runs tests that");
  puts("                      need owner permissions.");
  puts("  --owner_password - used to provide an owner password");
}

int Startup() {
  trunks::TrunksFactoryImpl factory;
  factory.GetTpmUtility()->Shutdown();
  return factory.GetTpmUtility()->Startup();
}

int Clear() {
  trunks::TrunksFactoryImpl factory;
  return factory.GetTpmUtility()->Clear();
}

int InitializeTpm() {
  trunks::TrunksFactoryImpl factory;
  return factory.GetTpmUtility()->InitializeTpm();
}

int AllocatePCR() {
  trunks::TrunksFactoryImpl factory;
  trunks::TPM_RC result;
  result = factory.GetTpmUtility()->AllocatePCR("");
  if (result != trunks::TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error allocating PCR:" << trunks::GetErrorString(result);
    return result;
  }
  factory.GetTpmUtility()->Shutdown();
  return factory.GetTpmUtility()->Startup();
}

int TakeOwnership(const std::string& owner_password) {
  trunks::TrunksFactoryImpl factory;
  trunks::TPM_RC rc;
  rc = factory.GetTpmUtility()->TakeOwnership(owner_password,
                                              owner_password,
                                              owner_password);
  if (rc) {
    LOG(ERROR) << "Error taking ownership: " << trunks::GetErrorString(rc);
    return rc;
  }
  return 0;
}

int DumpStatus() {
  trunks::TrunksFactoryImpl factory;
  scoped_ptr<trunks::TpmState> state = factory.GetTpmState();
  trunks::TPM_RC result = state->Initialize();
  if (result != trunks::TPM_RC_SUCCESS) {
    LOG(ERROR) << "Failed to read TPM state: "
               << trunks::GetErrorString(result);
    return result;
  }
  printf("Owner password set: %s\n",
         state->IsOwnerPasswordSet() ? "true" : "false");
  printf("Endorsement password set: %s\n",
         state->IsEndorsementPasswordSet() ? "true" : "false");
  printf("Lockout password set: %s\n",
         state->IsLockoutPasswordSet() ? "true" : "false");
  printf("In lockout: %s\n",
         state->IsInLockout() ? "true" : "false");
  printf("Platform hierarchy enabled: %s\n",
         state->IsOwnerPasswordSet() ? "true" : "false");
  printf("Was shutdown orderly: %s\n",
         state->IsOwnerPasswordSet() ? "true" : "false");
  printf("Is RSA supported: %s\n",
         state->IsRSASupported() ? "true" : "false");
  printf("Is ECC supported: %s\n",
         state->IsECCSupported() ? "true" : "false");
  return 0;
}

}  // namespace

int main(int argc, char **argv) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  base::CommandLine *cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch("status")) {
    return DumpStatus();
  }
  if (cl->HasSwitch("startup")) {
    return Startup();
  }
  if (cl->HasSwitch("clear")) {
    return Clear();
  }
  if (cl->HasSwitch("init_tpm")) {
    return InitializeTpm();
  }
  if (cl->HasSwitch("allocate_pcr")) {
    return AllocatePCR();
  }
  if (cl->HasSwitch("help")) {
    puts("Trunks Client: A command line tool to access the TPM.");
    PrintUsage();
    return 0;
  }
  if (cl->HasSwitch("own")) {
    return TakeOwnership(cl->GetSwitchValueASCII("owner_password"));
  }
  if (cl->HasSwitch("regression_test")) {
    trunks::TrunksClientTest test;
    LOG(INFO) << "Running RNG test.";
    CHECK(test.RNGTest()) << "Error running RNGtest.";
    LOG(INFO) << "Running RSA key tests.";
    CHECK(test.SignTest()) << "Error running SignTest.";
    CHECK(test.DecryptTest()) << "Error running DecryptTest.";
    CHECK(test.ImportTest()) << "Error running ImportTest.";
    CHECK(test.AuthChangeTest()) << "Error running AuthChangeTest.";
    LOG(INFO) << "Running PCR tests.";
    CHECK(test.PCRTest()) << "Error running PCRTest.";
    LOG(INFO) << "Running Policy tests.";
    CHECK(test.PolicyAuthValueTest()) << "Error running PolicyAuthValueTest.";
    CHECK(test.PolicyAndTest()) << "Error running PolicyAndTest.";
    CHECK(test.PolicyOrTest()) << "Error running PolicyOrTest.";

    if (cl->HasSwitch("owner_password")) {
      std::string owner_password = cl->GetSwitchValueASCII("owner_password");
      LOG(INFO) << "Running NvramTest.";
      CHECK(test.NvramTest(owner_password)) << "Error running NvramTest.";
    }
    LOG(INFO) << "All tests were run successfully.";
    return 0;
  }
  puts("Invalid options!");
  PrintUsage();
  return -1;
}
