// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace cryptohome {

class HomeDirs;
class Platform;

// This class handles recovery of encrypted data from the stateful partition
// when necessary. Right now, data on the stateful partition is encrypted under
// a PCR0-bound key stored in the TPM. Unfortunately, for RMAed devices, this
// makes extracting system logs that we need to do diagnosis pretty irritating,
// so this class supplies a backdoor, authenticated by (a hash of) the machine
// owner's passphrase. The flow is:
// If there is a request file (see the .cc file for the exact path) present AND
// it contains a passkey (in the PasswordToPasskey sense, see crypto.cc) AND
// that passkey is correct for the owner's cryptohome AND firmware write protect
// is disabled, copy the contents of /mnt/stateful/encrypted to
// /mnt/stateful/decrypted.
class StatefulRecovery {
 public:
  explicit StatefulRecovery(Platform* platform, HomeDirs* homedirs);
  ~StatefulRecovery();

  virtual void RecoverIfNeeded();
 private:
  // Returns true if we should recover stateful contents.
  bool ShouldRecover();
  // Returns true if it successfully recovered stateful contents.
  bool Recover();

  // Check for and parse the aforementioned request flag file.
  bool ParseFlagFile();
  // Return whether the auth token we got from the flag file is good. Currently,
  // this is the owner's passkey.
  bool IsAuthTokenValid();
  // Return whether the system's firmware is currently write-protected.
  bool IsFirmwareWriteProtected();

  std::string authtoken_;
  std::string destpath_;

  Platform* platform_;
  HomeDirs* homedirs_;

  static const char *kRecoverSource;
  static const char *kRecoverDestination;
  static const char *kFlagFile;
};

}  // namespace cryptohome
