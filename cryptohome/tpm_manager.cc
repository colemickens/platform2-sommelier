// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/secure_blob.h>
#include <brillo/syslog_logging.h>
#include <openssl/evp.h>

#include "cryptohome/cryptohome_metrics.h"

namespace cryptohome {

namespace tpm_manager {

void PrintUsage() {
  std::string program =
      base::CommandLine::ForCurrentProcess()->GetProgram().BaseName().value();
  printf("Usage: %s [command] [options]\n", program.c_str());
  printf("  Commands:\n");
  printf("    initialize: Takes ownership of an unowned TPM and initializes it "
         "for use with Chrome OS Core. This is the default command.\n"
         "      - Install attributes will be empty and finalized.\n"
         "      - Attestation data will be prepared.\n"
         "      This command may be run safely multiple times and may be "
         "retried on failure. If the TPM is already initialized this command\n"
         "      has no effect and exits without error. The --finalize option "
         "will cause various TPM data to be finalized (this does not affect\n"
         "      install attributes which are always finalized).\n");
  printf("    verify_endorsement: Verifies TPM endorsement.\n"
         "      If the --cros_core option is specified then Chrome OS Core "
         "endorsement is verified. Otherwise, normal Chromebook endorsement\n"
         "      is verified. Requires the TPM to be initialized but not "
         "finalized.\n");
  printf("    dump_status: Prints TPM status information.\n");
  printf("    get_random <N>: Gets N random bytes from the TPM and prints them "
         "as a hex-encoded string.\n");
}

int TakeOwnership(bool finalize);
int VerifyEK(bool is_cros_core);
int DumpStatus();
int GetRandom(unsigned int random_bytes_count);

}  // namespace tpm_manager

}  // namespace cryptohome

using cryptohome::tpm_manager::PrintUsage;
using cryptohome::tpm_manager::TakeOwnership;
using cryptohome::tpm_manager::VerifyEK;
using cryptohome::tpm_manager::DumpStatus;
using cryptohome::tpm_manager::GetRandom;

int main(int argc, char **argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine *command_line = base::CommandLine::ForCurrentProcess();
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);
  OpenSSL_add_all_algorithms();
  cryptohome::ScopedMetricsInitializer metrics_initializer;
  base::CommandLine::StringVector arguments = command_line->GetArgs();
  std::string command;
  if (arguments.size() > 0) {
    command = arguments[0];
  }
  if (command_line->HasSwitch("h") || command_line->HasSwitch("help")) {
    PrintUsage();
    return 0;
  }
  if (command.empty() || command == "initialize") {
    return TakeOwnership(command_line->HasSwitch("finalize"));
  }
  if (command == "verify_endorsement") {
    return VerifyEK(command_line->HasSwitch("cros_core"));
  }
  if (command == "dump_status") {
    return DumpStatus();
  }
  unsigned int random_bytes_count = 0;
  if (command == "get_random" && arguments.size() == 2 &&
      base::StringToUint(arguments[1], &random_bytes_count) &&
      random_bytes_count > 0) {
    return GetRandom(random_bytes_count);
  }
  PrintUsage();
  return -1;
}
