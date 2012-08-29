// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/string_util.h>
#include <chromeos/utility.h>
#include <string>

#include "homedirs.h"
#include "platform.h"
#include "stateful_recovery.h"
#include "username_passkey.h"

using std::string;

namespace cryptohome {

const char *StatefulRecovery::kFlagFile =
    "/mnt/stateful_partition/decrypt_stateful_hack";
const char *StatefulRecovery::kRecoverSource =
    "/mnt/stateful_partition/encrypted";
const char *StatefulRecovery::kRecoverDestination =
    "/mnt/stateful_partition/decrypted";

StatefulRecovery::StatefulRecovery(Platform* platform, HomeDirs* homedirs)
    : platform_(platform), homedirs_(homedirs) { }
StatefulRecovery::~StatefulRecovery() { }

void StatefulRecovery::RecoverIfNeeded() {
  if (ShouldRecover())
    Recover();
}

bool StatefulRecovery::ShouldRecover() {
  if (!ParseFlagFile())
    return false;
  if (IsFirmwareWriteProtected())
    return false;
  if (!IsAuthTokenValid())
    return false;
  return true;
}

bool StatefulRecovery::Recover() {
  return platform_->Copy(kRecoverSource, kRecoverDestination);
}

bool StatefulRecovery::ParseFlagFile() {
  std::string contents;
  if (!platform_->ReadFileToString(kFlagFile, &contents))
    return false;
  std::vector<string> fields;
  // The two fields: version, authtoken
  if (Tokenize(contents, " ", &fields) != 2) {
    LOG(ERROR) << "Malformed decryption hack";
    return false;
  }
  if (fields[0] != "0") {
    // TODO(ellyjones): UMA stat?
    LOG(ERROR) << "Decryption hack version is bogus: " << fields[0];
    return false;
  }
  authtoken_ = fields[1];
  return true;
}

bool StatefulRecovery::IsAuthTokenValid() {
  // Current auth token format: hex encoding of the owner's passkey (as derived
  // by PasswordToPasskey). We test this passkey against the owner's account (as
  // stored in the system policy), and if it matches, the auth token is valid.
  string owner;
  if (!homedirs_->GetPlainOwner(&owner))
    // No system owner. Fail open.
    return true;
  chromeos::Blob passkey(authtoken_.begin(), authtoken_.end());
  UsernamePasskey up(owner.c_str(), passkey);
  if (!homedirs_->AreCredentialsValid(up)) {
    LOG(ERROR) << "Auth token invalid for user " << owner;
    return false;
  }
  return true;
}

bool StatefulRecovery::IsFirmwareWriteProtected() {
  if (system("/usr/bin/crossystem wpsw_boot?1") != 0) {
    LOG(ERROR) << "Firmware write protect not set.";
    return false;
  }
  return true;
}

}  // namespace cryptohome
