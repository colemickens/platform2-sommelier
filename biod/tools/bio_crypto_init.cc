// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a program to set the various biometric managers with a TPM
// seed obtained from the TPM hardware. It is expected to execute once
// on every boot.
// This binary is expected to be called from the mount-encrypted utility
// during boot.
// It is expected to receive the tpm seed buffer from mount-encrypted via a
// file written to tmpfs. The FD for the tmpfs file is mapped to STDIN_FILENO
// by mount-encrypted. It is considered to have been unlinked by
// mount-encrypted. Consequently, closing the FD should be enough to delete
// the file.

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/process/process.h>
#include <base/time/time.h>
#include <brillo/flag_helper.h>
#include <brillo/secure_blob.h>
#include <chromeos/ec/ec_commands.h>

#include "biod/cros_fp_device.h"

namespace {

constexpr int64_t kTimeoutSeconds = 30;
constexpr int64_t kTpmSeedSize = FP_CONTEXT_TPM_BYTES;
// File where the TPM seed is stored, that we have to read from.
constexpr char kBioTpmSeedTmpFile[] = "/run/bio_crypto_init/seed";

// Helper function to ensure data of a file is removed.
bool NukeFile(const base::FilePath& filepath) {
  // Write all zeros to the FD.
  bool ret = true;
  std::vector<uint8_t> zero_vec(kTpmSeedSize, 0);
  if (base::WriteFile(filepath, reinterpret_cast<const char*>(zero_vec.data()),
                      kTpmSeedSize) != kTpmSeedSize) {
    PLOG(ERROR) << "Failed to write all-zero to tmpfs file.";
    ret = false;
  }

  if (!base::DeleteFile(filepath, false)) {
    PLOG(ERROR) << "Failed to delete TPM seed file: " << filepath.value();
    ret = false;
  }

  return ret;
}

bool WriteSeedToCrosFp(const brillo::SecureBlob& seed) {
  bool ret = true;
  auto fd =
      base::ScopedFD(open(biod::CrosFpDevice::kCrosFpPath, O_RDWR | O_CLOEXEC));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Couldn't open FP device for ioctl.";
    return false;
  }

  if (!biod::CrosFpDevice::WaitOnEcBoot(fd, EC_IMAGE_RW)) {
    LOG(ERROR) << "FP device did not boot to RW.";
    return false;
  }

  biod::EcCommand<struct ec_params_fp_seed, biod::EmptyParam> cmd(
      EC_CMD_FP_SEED);
  struct ec_params_fp_seed* req = cmd.Req();
  req->struct_version = FP_TEMPLATE_FORMAT_VERSION;
  std::copy(seed.char_data(), seed.char_data() + sizeof(req->seed), req->seed);

  if (!cmd.Run(fd.get())) {
    LOG(ERROR) << "Failed to set TPM seed.";
    ret = false;
  } else {
    LOG(INFO) << "Successfully set FP seed.";
  }
  std::fill(req->seed, req->seed + sizeof(req->seed), 0);
  // Clear intermediate buffers. We expect the command to fail since the SBP
  // will reject the new seed.
  cmd.Run(fd.get());

  return ret;
}

bool DoProgramSeed(const brillo::SecureBlob& tpm_seed) {
  bool ret = true;

  if (!WriteSeedToCrosFp(tpm_seed)) {
    LOG(ERROR) << "Failed to send seed to CrOS FP device.";
    ret = false;
  }

  return ret;
}

}  // namespace

int main(int argc, char* argv[]) {
  // Set up logging settings.
  DEFINE_string(log_dir, "/var/log/", "Directory where logs are written.");

  brillo::FlagHelper::Init(argc, argv,
                           "bio_crypto_init, the Chromium OS binary to program "
                           "bio sensors with TPM secrets.");

  const base::FilePath log_file =
      base::FilePath(FLAGS_log_dir).Append(std::string("bio_crypto_init.log"));

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_FILE;
  logging_settings.log_file = log_file.value().c_str();
  logging_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  logging_settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::SetLogItems(true,    // process ID
                       true,    // thread ID
                       true,    // timestamp
                       false);  // tickcount
  logging::InitLogging(logging_settings);

  // The first thing we do is read the buffer, and delete the file.
  brillo::SecureBlob tpm_seed(kTpmSeedSize);
  int bytes_read = base::ReadFile(base::FilePath(kBioTpmSeedTmpFile),
                                  tpm_seed.char_data(), tpm_seed.size());
  NukeFile(base::FilePath(kBioTpmSeedTmpFile));

  if (bytes_read != kTpmSeedSize) {
    LOG(ERROR) << "Failed to read TPM seed from tmpfile: " << bytes_read;
    return -1;
  }

  // We fork the process so that can we program the seed in the child, and
  // terminate it if it hangs.
  pid_t pid = fork();
  if (pid == -1) {
    PLOG(ERROR) << "Failed to fork child process for bio_wash.";
    return -1;
  }

  if (pid == 0) {
    return DoProgramSeed(tpm_seed) ? 0 : -1;
  }

  auto process = base::Process::Open(pid);
  int exit_code;
  if (!process.WaitForExitWithTimeout(
          base::TimeDelta::FromSeconds(kTimeoutSeconds), &exit_code)) {
    LOG(ERROR) << "bio_crypto_init timeout, exit code: " << exit_code;
    process.Terminate(-1, false);
  }

  return exit_code;
}
