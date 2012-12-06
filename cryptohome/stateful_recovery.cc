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

#define MNT_STATEFUL_PARTITION "/mnt/stateful_partition/"

const char *StatefulRecovery::kFlagFile =
    MNT_STATEFUL_PARTITION "decrypt_stateful";
const char *StatefulRecovery::kRecoverSource =
    MNT_STATEFUL_PARTITION "encrypted";
const char *StatefulRecovery::kRecoverDestination =
    MNT_STATEFUL_PARTITION "decrypted";
const char *StatefulRecovery::kRecoverBlockUsage =
    MNT_STATEFUL_PARTITION "decrypted/block-usage.txt";
const char *StatefulRecovery::kRecoverInodeUsage =
    MNT_STATEFUL_PARTITION "decrypted/inode-usage.txt";
const char *StatefulRecovery::kRecoverFilesystemDetails =
    MNT_STATEFUL_PARTITION "decrypted/filesystem-details.txt";

StatefulRecovery::StatefulRecovery(Platform* platform)
    : requested_(false), platform_(platform) { }
StatefulRecovery::~StatefulRecovery() { }

bool StatefulRecovery::Requested() {
  requested_ = ParseFlagFile();
  return requested_;
}

bool StatefulRecovery::CopyPartitionInfo() {
  if (!platform_->ReportBlockUsage(kRecoverSource, kRecoverBlockUsage))
    return false;

  if (!platform_->ReportInodeUsage(kRecoverSource, kRecoverInodeUsage))
    return false;

  if (!platform_->ReportFilesystemDetails(kRecoverSource,
                                          kRecoverFilesystemDetails))
    return false;

  return true;
}

bool StatefulRecovery::CopyPartitionContents() {
  int rc;

  rc = platform_->Copy(kRecoverSource, kRecoverDestination);
  if (rc)
    return true;
  LOG(ERROR) << "Failed to copy " << kRecoverSource;
  return false;
}

bool StatefulRecovery::Recover() {
  if (!requested_)
    return false;
  if (!platform_->CreateDirectory(kRecoverDestination)) {
    LOG(ERROR) << "Failed to mkdir " << kRecoverDestination;
    return false;
  }
  if (!CopyPartitionContents())
    return false;
  if (!CopyPartitionInfo())
    return false;
  return true;
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
