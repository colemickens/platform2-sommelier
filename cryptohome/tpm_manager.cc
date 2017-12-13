// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <stdio.h>
#include <string>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
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
  printf("    get_version_info: Prints TPM software and hardware version "
         "information.\n");
  printf("    get_ifx_field_upgrade_info: Prints status information pertaining "
         "to firmware updates on Infineon TPMs.\n");
}

static void PrintIFXFirmwarePackage(
    const Tpm::IFXFieldUpgradeInfo::FirmwarePackage& firmware_package,
    const char* prefix) {
  printf("%s_package_id %08x\n", prefix, firmware_package.package_id);
  printf("%s_version %08x\n", prefix, firmware_package.version);
  printf("%s_stale_version %08x\n", prefix, firmware_package.stale_version);
}

int TakeOwnership(bool finalize);
int VerifyEK(bool is_cros_core);
int DumpStatus();
int GetRandom(unsigned int random_bytes_count);
int GetVersionInfo(Tpm::TpmVersionInfo* version_info);
int GetIFXFieldUpgradeInfo(Tpm::IFXFieldUpgradeInfo* info);

}  // namespace tpm_manager

}  // namespace cryptohome

using cryptohome::tpm_manager::DumpStatus;
using cryptohome::tpm_manager::GetIFXFieldUpgradeInfo;
using cryptohome::tpm_manager::GetRandom;
using cryptohome::tpm_manager::GetVersionInfo;
using cryptohome::tpm_manager::PrintIFXFirmwarePackage;
using cryptohome::tpm_manager::PrintUsage;
using cryptohome::tpm_manager::TakeOwnership;
using cryptohome::tpm_manager::VerifyEK;

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
  if (command == "get_version_info") {
    cryptohome::Tpm::TpmVersionInfo version_info;
    if (!GetVersionInfo(&version_info)) {
      return -1;
    }

    uint32_t fingerprint = version_info.GetFingerprint();
    std::string vendor_specific = base::ToLowerASCII(
        base::HexEncode(version_info.vendor_specific.data(),
                        version_info.vendor_specific.size()));
    printf("tpm_family %08" PRIx32 "\n"
           "spec_level %016" PRIx64 "\n"
           "vendor %08" PRIx32 "\n"
           "tpm_model %08" PRIx32 "\n"
           "firmware_version %016" PRIx64 "\n"
           "vendor_specific %s\n"
           "version_fingerprint %" PRId32 " %08" PRIx32 "\n",
           version_info.family,
           version_info.spec_level,
           version_info.manufacturer,
           version_info.tpm_model,
           version_info.firmware_version,
           vendor_specific.c_str(),
           fingerprint,
           fingerprint);
    return 0;
  }
  if (command == "get_ifx_field_upgrade_info") {
    cryptohome::Tpm::IFXFieldUpgradeInfo info;
    if (!GetIFXFieldUpgradeInfo(&info)) {
      return -1;
    }

    printf("max_data_size %u\n", info.max_data_size);
    PrintIFXFirmwarePackage(info.bootloader, "bootloader");
    PrintIFXFirmwarePackage(info.firmware[0], "fw0");
    PrintIFXFirmwarePackage(info.firmware[1], "fw1");
    printf("status %04x\n", info.status);
    PrintIFXFirmwarePackage(info.process_fw, "process_fw");
    printf("field_upgrade_counter %u\n", info.field_upgrade_counter);

    return 0;
  }
  PrintUsage();
  return -1;
}
