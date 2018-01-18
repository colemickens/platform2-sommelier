// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A tool that use bootlockbox to sign and verify data. For example:
// bootlockboxtool --action=sign --file=abc.txt
// This command generates a out of file signature abc.txt.signature.

#include <memory>

#include <stdlib.h>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "cryptohome/bootlockbox/boot_lockbox_client.h"

namespace {

constexpr char kActionSign[] = "sign";
constexpr char kActionVerify[] = "verify";
constexpr char kActionFinalize[] = "finalize";

}  // namespace

int main(int argc, char **argv) {
  DEFINE_string(action, "",
      "Choose one action [sign|verify|finalize] to perform.");
  DEFINE_string(file, "",
                "Choose the file which needs to be signed or verified.");
  brillo::FlagHelper::Init(argc, argv, "bootlockbox");

  brillo::OpenLog("bootlockbox", true);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);

  if (FLAGS_action.empty()) {
     LOG(ERROR) << "must specify one action: [sign|verify|finalize]";
    return EXIT_FAILURE;
  }

  if (FLAGS_action != kActionSign && FLAGS_action != kActionVerify &&
      FLAGS_action != kActionFinalize) {
    LOG(ERROR) << "Invalid action: [sign|verify|finalize].";
    return EXIT_FAILURE;
  }

  std::unique_ptr<cryptohome::BootLockboxClient> boot_lockbox_client =
      cryptohome::BootLockboxClient::CreateBootLockboxClient();

  if (FLAGS_action == kActionFinalize) {
    if (!boot_lockbox_client->Finalize()) {
      LOG(ERROR) << "Failed to finalize bootlockbox.";
      return EXIT_FAILURE;
    }
    LOG(INFO) << "Success.";
    return EXIT_SUCCESS;
  }

  // Needs a file for sign or verify.
  if (FLAGS_file.empty()) {
    LOG(ERROR) << "must specify one file to " << FLAGS_action;
    return EXIT_FAILURE;
  }

  std::string data;
  base::FilePath file_path(FLAGS_file);
  if (!base::ReadFileToString(file_path, &data)) {
    LOG(ERROR) << "Failed to read input file: " << file_path.value();
    return EXIT_FAILURE;
  }

  if (FLAGS_action == kActionSign) {
    std::string signature;
    if (!boot_lockbox_client->Sign(data, &signature)) {
      LOG(ERROR) << "Failed to sign, check log for more info";
      return EXIT_FAILURE;
    }
    base::FilePath out_file = file_path.AddExtension("signature");
    base::WriteFile(out_file, signature.data(), signature.size());
    LOG(INFO) << "SignBootLockbox success.";
  } else if (FLAGS_action == kActionVerify) {
    std::string signature;
    base::FilePath signature_file = file_path.AddExtension("signature");
    if (!base::ReadFileToString(signature_file,
                                &signature)) {
      LOG(ERROR) << "Failed to read signature file: " << signature_file.value();
      return EXIT_FAILURE;
    }
    if (!boot_lockbox_client->Verify(data, signature)) {
      LOG(ERROR) << "Failed to verify the signature.";
      return EXIT_FAILURE;
    }
    LOG(INFO) << "VerifyBootLockbox success.";
  }

  return EXIT_SUCCESS;
}
