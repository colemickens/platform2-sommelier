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

#include "cryptohome/attestation.h"
#include "cryptohome/boot_lockbox.h"
#include "cryptohome/crypto.h"
#include "cryptohome/cryptohome_metrics.h"
#include "cryptohome/install_attributes.h"
#include "cryptohome/platform.h"
#include "cryptohome/tpm.h"
#include "cryptohome/tpm_init.h"
#include "rpc.pb.h"  // NOLINT(build/include)

namespace {

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

int TakeOwnership(bool finalize) {
  base::Time start_time = base::Time::Now();
  cryptohome::Platform platform;
  cryptohome::Tpm* tpm = cryptohome::Tpm::GetSingleton();
  cryptohome::TpmInit tpm_init(tpm, &platform);
  tpm_init.SetupTpm(false);
  LOG(INFO) << "Initializing TPM.";
  bool took_ownership = false;
  if (!tpm_init.InitializeTpm(&took_ownership)) {
    LOG(ERROR) << "Failed to initialize TPM.";
    return -1;
  }
  cryptohome::InstallAttributes install_attributes(tpm);
  if (took_ownership && !install_attributes.PrepareSystem()) {
    LOG(ERROR) << "Failed to prepare install attributes NVRAM.";
    return -1;
  }
  if (!install_attributes.Init(&tpm_init)) {
    LOG(ERROR) << "Failed to initialize install attributes.";
    return -1;
  }
  if (!install_attributes.Finalize()) {
    LOG(ERROR) << "Failed to finalize install attributes.";
    return -1;
  }
  cryptohome::Crypto crypto(&platform);
  crypto.set_use_tpm(true);
  crypto.Init(&tpm_init);
  cryptohome::Attestation attestation;
  attestation.Initialize(tpm, &tpm_init, &platform, &crypto,
                         &install_attributes,
                         brillo::SecureBlob(), /* abe_data */
                         true /* retain_endorsement_data */);
  attestation.PrepareForEnrollment();
  if (!attestation.IsPreparedForEnrollment()) {
    LOG(ERROR) << "Failed to initialize attestation.";
    return -1;
  }
  if (finalize) {
    attestation.FinalizeEndorsementData();
    tpm_init.ClearStoredTpmPassword();
  }
  base::TimeDelta duration = base::Time::Now() - start_time;
  LOG(INFO) << "TPM initialization successful ("
            << duration.InMilliseconds() << " ms).";
  return 0;
}

int VerifyEK(bool is_cros_core) {
  cryptohome::Platform platform;
  cryptohome::Tpm* tpm = cryptohome::Tpm::GetSingleton();
  cryptohome::TpmInit tpm_init(tpm, &platform);
  tpm_init.SetupTpm(false);
  cryptohome::InstallAttributes install_attributes(tpm);
  install_attributes.Init(&tpm_init);
  cryptohome::Crypto crypto(&platform);
  crypto.set_use_tpm(true);
  crypto.Init(&tpm_init);
  cryptohome::Attestation attestation;
  attestation.Initialize(tpm, &tpm_init, &platform, &crypto,
                         &install_attributes,
                         brillo::SecureBlob(), /* abe_data */
                         true /* retain_endorsement_data */);
  if (!attestation.VerifyEK(is_cros_core)) {
    LOG(ERROR) << "Failed to verify TPM endorsement.";
    return -1;
  }
  LOG(INFO) << "TPM endorsement verified successfully.";
  return 0;
}

int DumpStatus() {
  cryptohome::Platform platform;
  cryptohome::Tpm* tpm = cryptohome::Tpm::GetSingleton();
  cryptohome::TpmInit tpm_init(tpm, &platform);
  tpm_init.SetupTpm(false);
  cryptohome::GetTpmStatusReply status;
  status.set_enabled(tpm_init.IsTpmEnabled());
  status.set_owned(tpm_init.IsTpmOwned());
  brillo::SecureBlob owner_password;
  if (tpm_init.GetTpmPassword(&owner_password)) {
    status.set_initialized(false);
    status.set_owner_password(owner_password.to_string());
  } else {
    // Initialized is true only when the TPM is owned and the owner password has
    // already been destroyed.
    status.set_initialized(status.owned());
  }
  int counter;
  int threshold;
  bool lockout;
  int seconds_remaining;
  if (tpm->GetDictionaryAttackInfo(&counter, &threshold, &lockout,
                                   &seconds_remaining)) {
    status.set_dictionary_attack_counter(counter);
    status.set_dictionary_attack_threshold(threshold);
    status.set_dictionary_attack_lockout_in_effect(lockout);
    status.set_dictionary_attack_lockout_seconds_remaining(seconds_remaining);
  }

  cryptohome::InstallAttributes install_attributes(tpm);
  install_attributes.Init(&tpm_init);
  status.set_install_lockbox_finalized(
      status.owned() &&
      !install_attributes.is_first_install() &&
      !install_attributes.is_invalid() &&
      install_attributes.is_initialized());

  cryptohome::Crypto crypto(&platform);
  crypto.set_use_tpm(true);
  crypto.Init(&tpm_init);
  cryptohome::Attestation attestation;
  attestation.Initialize(tpm, &tpm_init, &platform, &crypto,
                         &install_attributes,
                         brillo::SecureBlob(), /* abe_data */
                         true /* retain_endorsement_data */);
  status.set_attestation_prepared(attestation.IsPreparedForEnrollment());
  status.set_attestation_enrolled(attestation.IsEnrolled());
  status.set_verified_boot_measured(attestation.IsPCR0VerifiedMode());

  cryptohome::BootLockbox boot_lockbox(tpm, &platform, &crypto);
  status.set_boot_lockbox_finalized(boot_lockbox.IsFinalized());

  status.PrintDebugString();
  return 0;
}

int GetRandom(unsigned int random_bytes_count) {
  cryptohome::Tpm* tpm = cryptohome::Tpm::GetSingleton();
  brillo::SecureBlob random_bytes;
  tpm->GetRandomData(random_bytes_count, &random_bytes);
  if (random_bytes_count != random_bytes.size())
    return -1;

  std::string hex_bytes =
      base::HexEncode(random_bytes.data(), random_bytes.size());
  printf("%s\n", hex_bytes.c_str());
  return 0;
}

}  // namespace

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
