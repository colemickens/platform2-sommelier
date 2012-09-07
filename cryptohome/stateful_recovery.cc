// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides the implementation of StatefulRecovery.

#include <unistd.h>
#include <linux/reboot.h>
#include <sys/reboot.h>

#include <base/string_util.h>
#include <chromeos/utility.h>
#include <string>

#include "platform.h"
#include "stateful_recovery.h"

using std::string;

namespace cryptohome {

const char *StatefulRecovery::kFlagFile =
    "/mnt/stateful_partition/decrypt_stateful";
const char *StatefulRecovery::kRecoverSource =
    "/mnt/stateful_partition/encrypted";
const char *StatefulRecovery::kRecoverDestination =
    "/mnt/stateful_partition/decrypted";

StatefulRecovery::StatefulRecovery(Platform* platform)
    : requested_(false), platform_(platform) { }
StatefulRecovery::~StatefulRecovery() { }

bool StatefulRecovery::Requested() {
  requested_ = ParseFlagFile();
  return requested_;
}

bool StatefulRecovery::Recover() {
  if (!requested_)
    return false;
  return platform_->Copy(kRecoverSource, kRecoverDestination);
}

void StatefulRecovery::PerformReboot() {
  // TODO(wad) Replace with a mockable helper.
  if (system("/usr/bin/crossystem recovery_request=1") != 0) {
    LOG(ERROR) << "Failed to set recovery request!";
  }
  sync();
  reboot(LINUX_REBOOT_CMD_RESTART);
}

bool StatefulRecovery::ParseFlagFile() {
  std::string contents;
  if (!platform_->ReadFileToString(kFlagFile, &contents))
    return false;
  // One field at present -- the version.
  if (contents != "1") {
    // TODO(ellyjones): UMA stat?
    LOG(ERROR) << "Bogus stateful recovery request file: " << contents;
    return false;
  }
  return true;
}

}  // namespace cryptohome
